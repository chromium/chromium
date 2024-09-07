// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/timer/timer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/top_chrome/per_profile_webui_tracker.h"
#include "chrome/browser/ui/webui/top_chrome/preload_context.h"
#include "chrome/browser/ui/webui/top_chrome/profile_preload_candidate_selector.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/models/menu_model.h"
#include "url/url_constants.h"

namespace {

// Enum class representing the results of attempting to use a preloaded WebUI
// when WebUIContentsPreloadedManager::Request() is called.
// The description of each value is also in tools/metrics/histograms/enums.xml.
enum class WebUIPreloadResult {
  // No preloaded WebUI is available when a WebUI is requested.
  kNoPreload = 0,
  // The preloaded WebUI matches the requested WebUI.
  kHit = 1,
  // The preloaded WebUI is redirected to the requested WebUI.
  kHitRedirected = 2,
  // The preloaded WebUI does not match the requested WebUI and cannot be
  // redirected.
  kMiss = 3,
  kMaxValue = kMiss,
};

// A candidate selector that always preloads a fixed WebUI.
class FixedCandidateSelector : public webui::PreloadCandidateSelector {
 public:
  explicit FixedCandidateSelector(GURL webui_url) : webui_url_(webui_url) {}
  FixedCandidateSelector(const FixedCandidateSelector&) = default;
  FixedCandidateSelector(FixedCandidateSelector&&) = default;
  FixedCandidateSelector& operator=(const FixedCandidateSelector&) = default;
  FixedCandidateSelector& operator=(FixedCandidateSelector&&) = default;

  // webui::PreloadCandidateSelector:
  void Init(const std::vector<GURL>& preloadable_urls) override {
    DCHECK(base::Contains(preloadable_urls, webui_url_));
  }
  std::optional<GURL> GetURLToPreload(
      const webui::PreloadContext& context) const override {
    return webui_url_;
  }

 private:
  GURL webui_url_;
};

bool IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kPreloadTopChromeWebUI);
}

bool IsSmartPreloadEnabled() {
  return IsFeatureEnabled() &&
         features::kPreloadTopChromeWebUISmartPreload.Get();
}

bool IsDelayPreloadEnabled() {
  return IsFeatureEnabled() &&
         features::kPreloadTopChromeWebUIDelayPreload.Get();
}

content::WebContents::CreateParams GetWebContentsCreateParams(
    const GURL& webui_url,
    content::BrowserContext* browser_context) {
  content::WebContents::CreateParams create_params(browser_context);
  // Set it to visible so that the resources are immediately loaded.
  create_params.initially_hidden = !IsFeatureEnabled();
  create_params.site_instance =
      content::SiteInstance::CreateForURL(browser_context, webui_url);

  return create_params;
}

content::WebUIController* GetWebUIController(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  content::WebUI* webui = web_contents->GetWebUI();
  if (!webui) {
    return nullptr;
  }

  return webui->GetController();
}

}  // namespace

// A stub WebUI page embdeder that captures the ready-to-show signal.
class WebUIContentsPreloadManager::WebUIControllerEmbedderStub final
    : public TopChromeWebUIController::Embedder {
 public:
  WebUIControllerEmbedderStub() = default;
  ~WebUIControllerEmbedderStub() = default;

  // TopChromeWebUIController::Embedder:
  void CloseUI() override {}
  void ShowContextMenu(gfx::Point point,
                       std::unique_ptr<ui::MenuModel> menu_model) override {}
  void HideContextMenu() override {}
  void ShowUI() override { is_ready_to_show_ = true; }

  // Attach this stub as the embedder of `web_contents`, assuming that the
  // contents is not yet ready to be shown.
  void AttachTo(content::WebContents* web_contents) {
    CHECK_NE(web_contents, nullptr);
    content::WebUIController* webui_controller =
        GetWebUIController(web_contents);
    if (!webui_controller) {
      return;
    }
    // TODO(40168622): Add type check. This is currently not possible because a
    // WebUIController subclass does not retain its parent class' type info.
    auto* bubble_controller =
        static_cast<TopChromeWebUIController*>(webui_controller);
    bubble_controller->set_embedder(this->GetWeakPtr());
    web_contents_ = web_contents;
    is_ready_to_show_ = false;
  }

  // Detach from the previously attached `web_contents`.
  void Detach() {
    if (!web_contents_) {
      return;
    }

    content::WebUIController* webui_controller =
        GetWebUIController(web_contents_);
    if (!webui_controller) {
      return;
    }

    auto* bubble_controller =
        static_cast<TopChromeWebUIController*>(webui_controller);
    bubble_controller->set_embedder(nullptr);
    web_contents_ = nullptr;
  }

  bool is_ready_to_show() const { return is_ready_to_show_; }

  base::WeakPtr<WebUIControllerEmbedderStub> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  bool is_ready_to_show_ = false;
  base::WeakPtrFactory<WebUIControllerEmbedderStub> weak_ptr_factory_{this};
};

class WebUIContentsPreloadManager::PendingPreload
    : public content::WebContentsObserver,
      public ProfileObserver {
 public:
  // Notifies the manager to preload when `busy_web_contents_to_watch` emits the
  // first non-empty paint or when `deadline` has passed.
  PendingPreload(WebUIContentsPreloadManager* manager,
                 Profile* profile,
                 content::WebContents* busy_web_contents_to_watch,
                 base::TimeDelta deadline)
      : manager_(manager), profile_(profile) {
    WebContentsObserver::Observe(busy_web_contents_to_watch);
    profile_observation_.Observe(profile_);
    deadline_timer_.Start(FROM_HERE, deadline, this, &PendingPreload::Preload);
  }

  void Preload() {
    deadline_timer_.Stop();
    WebContentsObserver::Observe(nullptr);
    manager_->MaybePreloadForBrowserContext(profile_);
  }

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override { Preload(); }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    profile_observation_.Reset();
    profile_ = nullptr;
    WebContentsObserver::Observe(nullptr);
    deadline_timer_.Stop();
  }

 private:
  raw_ptr<WebUIContentsPreloadManager> manager_;
  raw_ptr<Profile> profile_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::OneShotTimer deadline_timer_;
};

using RequestResult = WebUIContentsPreloadManager::RequestResult;

RequestResult::RequestResult() = default;
RequestResult::~RequestResult() = default;
RequestResult::RequestResult(RequestResult&&) = default;
RequestResult& RequestResult::operator=(RequestResult&&) = default;

WebUIContentsPreloadManager::WebUIContentsPreloadManager() {
  preload_mode_ =
      static_cast<PreloadMode>(features::kPreloadTopChromeWebUIMode.Get());
  webui_controller_embedder_stub_ =
      std::make_unique<WebUIControllerEmbedderStub>();

  webui_tracker_ = PerProfileWebUITracker::Create();
  webui_tracker_observation_.Observe(webui_tracker_.get());
  if (IsSmartPreloadEnabled()) {
    // Use ProfilePreloadCandidateSelector to find the WebUI with the
    // highest engagement score and is not present under the current profile.
    SetPreloadCandidateSelector(
        std::make_unique<webui::ProfilePreloadCandidateSelector>(
            webui_tracker_.get()));
  } else {
    // Old behavior always preloads Tab Search.
    SetPreloadCandidateSelector(std::make_unique<FixedCandidateSelector>(
        GURL(chrome::kChromeUITabSearchURL)));
  }
}

WebUIContentsPreloadManager::~WebUIContentsPreloadManager() = default;

// static
WebUIContentsPreloadManager* WebUIContentsPreloadManager::GetInstance() {
  static base::NoDestructor<WebUIContentsPreloadManager> s_instance;
  return s_instance.get();
}

void WebUIContentsPreloadManager::WarmupForBrowser(Browser* browser) {
  // Most WebUIs, if not all, are hosted by a TYPE_NORMAL browser. This check
  // skips unnecessary preloading for the majority of WebUIs.
  if (!browser->is_type_normal()) {
    return;
  }

  if (preload_mode_ == PreloadMode::kPreloadOnMakeContents) {
    return;
  }

  CHECK_EQ(preload_mode_, PreloadMode::kPreloadOnWarmup);

  if (IsDelayPreloadEnabled()) {
    MaybePreloadForBrowserContextLater(
        browser->profile(), browser->tab_strip_model()->GetActiveWebContents());
  } else {
    MaybePreloadForBrowserContext(browser->profile());
  }
}

std::optional<GURL> WebUIContentsPreloadManager::GetNextWebUIURLToPreload(
    content::BrowserContext* browser_context) const {
  return preload_candidate_selector_->GetURLToPreload(
      webui::PreloadContext::From(
          Profile::FromBrowserContext(browser_context)));
}

std::vector<GURL> WebUIContentsPreloadManager::GetAllPreloadableWebUIURLs() {
  // Retrieves top-chrome WebUIs that enables IsPreloadable() in its WebUI
  // config.
  std::vector<GURL> preloadable_urls;
  TopChromeWebUIConfig::ForEachConfig([&preloadable_urls](
                                          TopChromeWebUIConfig* config) {
    if (config->IsPreloadable()) {
      preloadable_urls.emplace_back(base::StrCat(
          {config->scheme(), url::kStandardSchemeSeparator, config->host()}));
    }
  });

  return preloadable_urls;
}

void WebUIContentsPreloadManager::SetPreloadCandidateSelector(
    std::unique_ptr<webui::PreloadCandidateSelector>
        preload_candidate_selector) {
  preload_candidate_selector_ = std::move(preload_candidate_selector);
  if (preload_candidate_selector_) {
    preload_candidate_selector_->Init(GetAllPreloadableWebUIURLs());
  }
}

void WebUIContentsPreloadManager::MaybePreloadForBrowserContext(
    content::BrowserContext* browser_context) {
  pending_preload_.reset();

  if (!ShouldPreloadForBrowserContext(browser_context)) {
    return;
  }

  // Usually destroying a WebContents may trigger preload, but if the
  // destroy is caused by setting new preload contents, ignore it.
  if (is_setting_preloaded_web_contents_) {
    return;
  }

  std::optional<GURL> preload_url = GetNextWebUIURLToPreload(browser_context);
  if (!preload_url.has_value()) {
    return;
  }

  // Don't preload if already preloaded for this `browser_context`.
  if (preloaded_web_contents_ &&
      preloaded_web_contents_->GetBrowserContext() == browser_context &&
      preloaded_web_contents_->GetVisibleURL().GetWithEmptyPath() ==
          preload_url->GetWithEmptyPath()) {
    return;
  }

  SetPreloadedContents(CreateNewContents(browser_context, *preload_url));
}

void WebUIContentsPreloadManager::MaybePreloadForBrowserContextLater(
    content::BrowserContext* browser_context,
    content::WebContents* busy_web_contents_to_watch,
    base::TimeDelta deadline) {
  pending_preload_ = std::make_unique<PendingPreload>(
      this, Profile::FromBrowserContext(browser_context),
      busy_web_contents_to_watch, deadline);
}

void WebUIContentsPreloadManager::SetPreloadedContents(
    std::unique_ptr<content::WebContents> web_contents) {
  webui_controller_embedder_stub_->Detach();
  profile_observation_.Reset();

  base::AutoReset<bool> is_setting_preloaded_web_contents(
      &is_setting_preloaded_web_contents_, true);
  preloaded_web_contents_ = std::move(web_contents);
  if (preloaded_web_contents_) {
    webui_controller_embedder_stub_->AttachTo(preloaded_web_contents_.get());
    profile_observation_.Observe(Profile::FromBrowserContext(
        preloaded_web_contents_->GetBrowserContext()));
  }
}

RequestResult WebUIContentsPreloadManager::Request(
    const GURL& webui_url,
    content::BrowserContext* browser_context) {
  const base::TimeTicks request_time = base::TimeTicks::Now();
  std::unique_ptr<content::WebContents> web_contents_ret;
  bool is_ready_to_show = false;
  WebUIPreloadResult preload_result = preloaded_web_contents_
                                          ? WebUIPreloadResult::kMiss
                                          : WebUIPreloadResult::kNoPreload;

  // Use preloaded contents if requested the same WebUI under the same browser
  // context. Navigating to or from a blank page is also allowed.
  // TODO(325836830): allow navigations between WebUIs.
  if (preloaded_web_contents_ &&
      preloaded_web_contents_->GetBrowserContext() == browser_context &&
      (preloaded_web_contents_->GetURL().host() == webui_url.host() ||
       preloaded_web_contents_->GetURL().IsAboutBlank() ||
       webui_url.IsAboutBlank())) {
    preload_result = WebUIPreloadResult::kHit;
    // Redirect if requested a different URL.
    if (!url::IsSameOriginWith(preloaded_web_contents_->GetURL(), webui_url)) {
      preload_result = WebUIPreloadResult::kHitRedirected;
      LoadURLForContents(preloaded_web_contents_.get(), webui_url);
    }
    web_contents_ret = std::move(preloaded_web_contents_);
    is_ready_to_show = webui_controller_embedder_stub_->is_ready_to_show();
    SetPreloadedContents(nullptr);
  } else {
    web_contents_ret = CreateNewContents(browser_context, webui_url);
    is_ready_to_show = false;
  }

  // Navigate to path if the request URL has a different path.
  if (!is_navigation_disabled_for_test_ &&
      webui_url.path() != web_contents_ret->GetURL().path()) {
    CHECK(url::IsSameOriginWith(webui_url, web_contents_ret->GetURL()));
    LoadURLForContents(web_contents_ret.get(), webui_url);
  }

  base::UmaHistogramEnumeration("WebUI.TopChrome.Preload.Result",
                                preload_result);

  // Preload a new contents.
  if (IsDelayPreloadEnabled()) {
    MaybePreloadForBrowserContextLater(browser_context, web_contents_ret.get());
  } else {
    MaybePreloadForBrowserContext(browser_context);
  }

  task_manager::WebContentsTags::ClearTag(web_contents_ret.get());
  request_time_map_[web_contents_ret.get()] = request_time;

  RequestResult result;
  result.web_contents = std::move(web_contents_ret);
  result.is_ready_to_show = is_ready_to_show;
  return result;
}

std::optional<base::TimeTicks> WebUIContentsPreloadManager::GetRequestTime(
    content::WebContents* web_contents) {
  return base::Contains(request_time_map_, web_contents)
             ? std::make_optional(request_time_map_[web_contents])
             : std::nullopt;
}

void WebUIContentsPreloadManager::DisableNavigationForTesting() {
  is_navigation_disabled_for_test_ = true;
}

std::unique_ptr<content::WebContents>
WebUIContentsPreloadManager::CreateNewContents(
    content::BrowserContext* browser_context,
    GURL url) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          GetWebContentsCreateParams(url, browser_context));

  // Propagates user prefs to web contents.
  // This is needed by, for example, text selection color on ChromeOS.
  PrefsTabHelper::CreateForWebContents(web_contents.get());

  task_manager::WebContentsTags::CreateForToolContents(
      web_contents.get(), IDS_TASK_MANAGER_PRELOADED_RENDERER_FOR_UI);
  chrome::InitializePageLoadMetricsForWebContents(web_contents.get());
  webui_tracker_->AddWebContents(web_contents.get());

  LoadURLForContents(web_contents.get(), url);

  return web_contents;
}

void WebUIContentsPreloadManager::LoadURLForContents(
    content::WebContents* web_contents,
    GURL url) {
  if (is_navigation_disabled_for_test_) {
    return;
  }

  web_contents->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
}

bool WebUIContentsPreloadManager::ShouldPreloadForBrowserContext(
    content::BrowserContext* browser_context) const {
  // Don't preload if the feature is disabled.
  if (!IsFeatureEnabled()) {
    return false;
  }

  if (browser_context->ShutdownStarted()) {
    return false;
  }

  // Don't preload if under heavy memory pressure.
  const auto* memory_monitor = base::MemoryPressureMonitor::Get();
  if (memory_monitor && memory_monitor->GetCurrentPressureLevel() >=
                            base::MemoryPressureMonitor::MemoryPressureLevel::
                                MEMORY_PRESSURE_LEVEL_MODERATE) {
    return false;
  }

  return true;
}

void WebUIContentsPreloadManager::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  if (!preloaded_web_contents_) {
    return;
  }

  webui_controller_embedder_stub_->Detach();
  CHECK_EQ(preloaded_web_contents_->GetBrowserContext(), profile);
  preloaded_web_contents_.reset();
}

void WebUIContentsPreloadManager::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  // Triggers preloading when a WebUI is destroyed. Without this step, the
  // preloaded content would only be the second highest engaged WebUI for
  // the most time.
  if (IsDelayPreloadEnabled()) {
    MaybePreloadForBrowserContextLater(web_contents->GetBrowserContext(),
                                       nullptr);
  } else {
    MaybePreloadForBrowserContext(web_contents->GetBrowserContext());
  }
  request_time_map_.erase(web_contents);
}

void WebUIContentsPreloadManager::OnWebContentsPrimaryPageChanged(
    content::WebContents* web_contents) {
  if (web_contents == preloaded_web_contents_.get()) {
    content::RenderWidgetHostView* render_widget_host_view =
        web_contents->GetRenderWidgetHostView();
    const bool should_auto_reisze_host =
        TopChromeWebUIConfig::From(web_contents->GetBrowserContext(),
                                   web_contents->GetVisibleURL())
            ->ShouldAutoResizeHost();
    if (render_widget_host_view && should_auto_reisze_host) {
      render_widget_host_view->EnableAutoResize(gfx::Size(1, 1),
                                                gfx::Size(INT_MAX, INT_MAX));
    }
  }
}
