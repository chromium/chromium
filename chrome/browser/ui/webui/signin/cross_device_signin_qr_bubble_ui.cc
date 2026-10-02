// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/cross_device_signin_qr_bubble_ui.h"

#include <string>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "chrome/grit/signin_resources_map.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/url_util.h"
#include "ui/gfx/image/image.h"
#include "ui/webui/webui_util.h"

CrossDeviceSigninQrBubbleUIConfig::CrossDeviceSigninQrBubbleUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUICrossDeviceSigninQrBubbleHost) {
  // NOLINT(modernize-use-equals-default): DefaultWebUIConfig requires
  // arguments.
}

bool CrossDeviceSigninQrBubbleUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop);
}

namespace {

class CrossDeviceSigninQrBubbleHandler
    : public cross_device_signin::mojom::PageHandler {
 public:
  CrossDeviceSigninQrBubbleHandler(
      mojo::PendingReceiver<cross_device_signin::mojom::PageHandler> receiver,
      Profile* profile,
      GURL qr_code_url)
      : receiver_(this, std::move(receiver)),
        profile_(profile),
        qr_code_url_(std::move(qr_code_url)) {}

  ~CrossDeviceSigninQrBubbleHandler() override = default;

  CrossDeviceSigninQrBubbleHandler(const CrossDeviceSigninQrBubbleHandler&) =
      delete;
  CrossDeviceSigninQrBubbleHandler& operator=(
      const CrossDeviceSigninQrBubbleHandler&) = delete;

  // cross_device_signin::mojom::PageHandler:
  void GetRegistrationData(GetRegistrationDataCallback callback) override {
    auto data =
        cross_device_signin::mojom::CrossDeviceSigninQrBubbleData::New();
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    if (!identity_manager) {
      std::move(callback).Run(nullptr);
      return;
    }
    CoreAccountInfo core_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (core_info.IsEmpty()) {
      std::move(callback).Run(nullptr);
      return;
    }
    data->email = core_info.email;

    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfo(core_info);
    data->full_name = std::string(account_info.GetFullName().value_or(""));

    std::optional<gfx::Image> avatar_image =
        account_info.IsEmpty() ? std::nullopt : account_info.GetAvatarImage();
    base::expected<gfx::ImageSkia, qr_code_generator::Error> qr_image;
    if (avatar_image.has_value()) {
      // Normalize the avatar to a fixed square size: the QR generator requires
      // a square center image and clamps it to a fraction of the code width,
      // so this only needs to be at least as large as that slot. Without this,
      // small avatars would be drawn at their native (tiny) size.
      constexpr int kQrCenterAvatarSize = 96;
      gfx::Image round_avatar = profiles::GetSizedAvatarIcon(
          avatar_image.value(), kQrCenterAvatarSize, kQrCenterAvatarSize,
          profiles::SHAPE_CIRCLE);
      qr_image = qr_code_generator::GenerateImage(
          base::as_byte_span(qr_code_url_.spec()),
          qr_code_generator::ModuleStyle::kCircles,
          qr_code_generator::LocatorStyle::kRounded, round_avatar.AsImageSkia(),
          qr_code_generator::QuietZone::kWillBeAddedByClient);
    } else {
      qr_image = qr_code_generator::GenerateImage(
          base::as_byte_span(qr_code_url_.spec()),
          qr_code_generator::ModuleStyle::kCircles,
          qr_code_generator::LocatorStyle::kRounded,
          qr_code_generator::CenterImage::kProductLogo,
          qr_code_generator::QuietZone::kWillBeAddedByClient);
    }

    if (qr_image.has_value()) {
      data->qr_code_data_uri =
          webui::EncodePNGAndMakeDataURI(qr_image.value(), 1.0f);
    }

    std::move(callback).Run(std::move(data));
  }

 private:
  mojo::Receiver<cross_device_signin::mojom::PageHandler> receiver_;
  raw_ptr<Profile> profile_;
  GURL qr_code_url_;
};

constexpr char kEntryPointParamKey[] = "entry_point";

CrossDeviceSigninPromoEntryPoint GetEntryPointFromUrl(const GURL& url) {
  std::string entry_point_str;
  int entry_point_int = 0;
  if (net::GetValueForKeyInQuery(url, kEntryPointParamKey, &entry_point_str) &&
      base::StringToInt(entry_point_str, &entry_point_int) &&
      entry_point_int >= 0 &&
      entry_point_int <=
          static_cast<int>(CrossDeviceSigninPromoEntryPoint::kMaxValue)) {
    return static_cast<CrossDeviceSigninPromoEntryPoint>(entry_point_int);
  }
  return CrossDeviceSigninPromoEntryPoint::kProfileMenu;
}

int GetBubbleSubtitleResourceId(CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_SUBTITLE;
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      return IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_BODY_QR;
  }
}

}  // namespace

CrossDeviceSigninQrBubbleUI::CrossDeviceSigninQrBubbleUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICrossDeviceSigninQrBubbleHost);

  webui::SetupWebUIDataSource(
      source, base::span(kSigninResources),
      IDR_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_CROSS_DEVICE_SIGNIN_QR_BUBBLE_HTML);

  // `content::WebContents::GetVisibleURL()` is used here because a
  // WebUIController is created before the navigation commits.
  const CrossDeviceSigninPromoEntryPoint entry_point =
      GetEntryPointFromUrl(web_ui->GetWebContents()->GetVisibleURL());

  source->UseStringsJs();
  source->AddLocalizedString("title", IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_TITLE);
  source->AddLocalizedString("subtitle",
                             GetBubbleSubtitleResourceId(entry_point));
  source->AddLocalizedString(
      "qrCodeAltText", IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_QR_CODE_ALT_TEXT);
  source->EnableReplaceI18nInJS();
}

CrossDeviceSigninQrBubbleUI::~CrossDeviceSigninQrBubbleUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CrossDeviceSigninQrBubbleUI)

// static
GURL CrossDeviceSigninQrBubbleUI::GetURLWithEntryPoint(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  return net::AppendQueryParameter(
      GURL(chrome::kChromeUICrossDeviceSigninQrBubbleURL), kEntryPointParamKey,
      base::NumberToString(static_cast<int>(entry_point)));
}

void CrossDeviceSigninQrBubbleUI::Initialize(GURL qr_code_url) {
  qr_code_url_ = std::move(qr_code_url);
}

void CrossDeviceSigninQrBubbleUI::BindInterface(
    mojo::PendingReceiver<cross_device_signin::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void CrossDeviceSigninQrBubbleUI::CreateCrossDeviceSigninQrBubbleHandler(
    mojo::PendingReceiver<cross_device_signin::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<CrossDeviceSigninQrBubbleHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()), qr_code_url_);
}
