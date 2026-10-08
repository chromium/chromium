// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tabstrip_ntb/tabstrip_ntb_ui.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/tabstrip_ntb_resources.h"
#include "chrome/grit/tabstrip_ntb_resources_map.h"
#include "components/browser_apis/tab_strip/tab_strip_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

#if !BUILDFLAG(OPTIMIZE_WEBUI)
#include "chrome/grit/tab_strip_api_resources_map.h"
#endif  // !BUILDFLAG(OPTIMIZE_WEBUI)

namespace tabstrip_ntb {

namespace {

// Associates a `tabs_api::TabStripService` with the WebContents hosting
// chrome://tabstrip-ntb.top-chrome/. If a `TabStripNtbUI` is created before
// there is a service, it leaves the service end of its pipe here, and the
// service picks it up when it is set.
class TabStripServiceUserData
    : public content::WebContentsUserData<TabStripServiceUserData> {
 public:
  ~TabStripServiceUserData() override = default;

  // Connects `service_end` to the service, or holds it until there is one.
  void Connect(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> service_end) {
    CHECK(!pending_service_end_.is_valid())
        << "TabStripServiceUserData already has a pending service end";
    pending_service_end_ = std::move(service_end);
    MaybeConnect();
  }

  void SetService(tabs_api::TabStripService* service) {
    service_ = service;
    MaybeConnect();
  }

 private:
  explicit TabStripServiceUserData(content::WebContents* web_contents)
      : content::WebContentsUserData<TabStripServiceUserData>(*web_contents) {}

  void MaybeConnect() {
    if (service_ && pending_service_end_.is_valid()) {
      service_->Accept(std::move(pending_service_end_));
    }
  }

  friend class content::WebContentsUserData<TabStripServiceUserData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  raw_ptr<tabs_api::TabStripService> service_ = nullptr;
  mojo::PendingReceiver<tabs_api::mojom::TabStripService> pending_service_end_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabStripServiceUserData);

}  // namespace

bool TabStripNtbUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      chrome::android::kAndroidNewTabButtonTabstripWebUI);
}

TabStripNtbUI::TabStripNtbUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui,
                               /*enable_chrome_send=*/false,
                               /*enable_chrome_histograms=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUITabStripNtbHost);
  webui::SetupWebUIDataSource(source, kTabstripNtbResources,
                              IDR_TABSTRIP_NTB_TABSTRIP_NTB_HTML);
#if !BUILDFLAG(OPTIMIZE_WEBUI)
  // Shared tabs_api TS bindings and helpers, served from /tab_strip_api/.
  // Optimized builds bundle them into app.rollup.js instead.
  source->AddResourcePaths(kTabStripApiResources);
#endif  // !BUILDFLAG(OPTIMIZE_WEBUI)

  source->AddLocalizedString("newTabButtonAccName", IDS_ACCNAME_NEWTAB);

  TabStripServiceUserData::GetOrCreateForWebContents(web_ui->GetWebContents())
      ->Connect(tab_strip_client_end_.InitWithNewPipeAndPassReceiver());
}

TabStripNtbUI::~TabStripNtbUI() = default;

// static
void TabStripNtbUI::SetTabStripServiceForWebContents(
    content::WebContents* web_contents,
    tabs_api::TabStripService* service) {
  CHECK(web_contents);
  TabStripServiceUserData::GetOrCreateForWebContents(web_contents)
      ->SetService(service);
}

void TabStripNtbUI::BindInterface(
    mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver) {
  CHECK(tab_strip_client_end_.is_valid())
      << "TabStripService client end already bound";
  CHECK(FusePipes(std::move(receiver), std::move(tab_strip_client_end_)));
}

WEB_UI_CONTROLLER_TYPE_IMPL(TabStripNtbUI)

}  // namespace tabstrip_ntb
