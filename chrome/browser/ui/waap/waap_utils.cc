// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_utils.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/frame_eviction_opt_out_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace {

// Lightweight `ColorProviderSource` for use during WebUI prewarming.
// During the prewarming phase, `WebContents` are created in the background
// before any `views::Widget` is instantiated. Since `WebContents` requires a
// `ColorProviderSource` to resolve theme colors, we use this class as a
// temporary placeholder that holds the calculated `ColorProviderKey` without
// relying on a full widget hierarchy.
class PrewarmColorProviderSource : public ui::ColorProviderSource,
                                   public base::SupportsUserData::Data {
 public:
  static constexpr char kUserDataKey[] = "prewarm_color_provider_source";

  explicit PrewarmColorProviderSource(ui::ColorProviderKey key) : key_(key) {}
  ~PrewarmColorProviderSource() override = default;

  // ui::ColorProviderSource:
  const ui::ColorProvider* GetColorProvider() const override {
    return ui::ColorProviderManager::Get().GetColorProviderFor(key_);
  }
  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    auto key = key_;
    key.color_mode = color_mode;
    key.forced_colors = forced_colors;
    const ui::ColorProvider* color_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(key);
    CHECK(color_provider);
    return ui::CreateRendererColorMap(*color_provider);
  }
  ui::ColorProviderKey GetColorProviderKey() const override { return key_; }

 private:
  const ui::ColorProviderKey key_;
};

}  // namespace

namespace waap {

bool IsForInitialWebUI(const GURL& url) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI) &&
      base::FeatureList::IsEnabled(features::kWebUIReloadButton)) {
    return url.SchemeIs(content::kChromeUIScheme) &&
           url.host() == chrome::kChromeUIWebUIToolbarHost;
  }
  return false;
}

void PrewarmHelper::ConfigureWebUIContents(content::WebContents* web_contents,
                                           Profile* profile) {
  // `PageLoadMetrics` needs to be initialized before loading the URL.
  InitializePageLoadMetricsForWebContents(web_contents);
  // Needed for UKM PageLoad metrics.
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents);

  web_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_contents->SetIgnoreZoomGestures(true);
  if (base::FeatureList::IsEnabled(
          features::kWebUIToolbarFrameEvictionOptOut)) {
    web_contents->OptOutFrameEviction(
        content::FrameEvictionOptOutClient::GetPassKey());
  }

  // Set up the color provider source so that early navigation can resolve
  // theme color properties.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
  ui::ColorProviderKey key = theme_service->GetColorProviderKey(
      ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(nullptr),
      profile);
  auto source = std::make_unique<PrewarmColorProviderSource>(key);
  web_contents->SetColorProviderSource(source.get());
  web_contents->SetUserData(PrewarmColorProviderSource::kUserDataKey,
                            std::move(source));
}

std::unique_ptr<content::WebContents> PrewarmHelper::PrewarmWebUIContents(
    Profile* profile,
    bool pre_navigate) {
  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::CreateForURL(
          profile, GURL(chrome::kChromeUIWebUIToolbarURL));
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile, site_instance));

  ConfigureWebUIContents(web_contents.get(), profile);

  if (pre_navigate) {
    web_contents->GetController().LoadURL(
        GURL(chrome::kChromeUIWebUIToolbarURL), content::Referrer(),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  } else {
    site_instance->GetProcess()->Init();
  }

  return web_contents;
}

}  // namespace waap
