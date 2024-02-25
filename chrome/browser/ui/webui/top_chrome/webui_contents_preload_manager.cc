// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"

#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/no_destructor.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/navigation_controller.h"

namespace {

// This factory is used to get notification for the browser context shutdown.
class BrowserContextShutdownNofifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  BrowserContextShutdownNofifierFactory(
      const BrowserContextShutdownNofifierFactory&) = delete;
  BrowserContextShutdownNofifierFactory& operator=(
      const BrowserContextShutdownNofifierFactory&) = delete;

  static BrowserContextShutdownNofifierFactory* GetInstance() {
    static base::NoDestructor<BrowserContextShutdownNofifierFactory> s_factory;
    return s_factory.get();
  }

 private:
  friend class base::NoDestructor<BrowserContextShutdownNofifierFactory>;
  BrowserContextShutdownNofifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "WebUIContentsPreloadManager") {}
};

content::WebContents::CreateParams GetWebContentsCreateParams(
    const GURL& webui_url,
    content::BrowserContext* browser_context) {
  content::WebContents::CreateParams create_params(browser_context);
  // Set it to visible so that the resources are immediately loaded.
  create_params.initially_hidden = false;
  create_params.site_instance =
      content::SiteInstance::CreateForURL(browser_context, webui_url);

  return create_params;
}

}  // namespace

// Currently we preloads Tab Search. In practice, this also benefits other
// WebUIs. This is likely due to reused render processes that increase cache
// hits and reduce re-creation of common structs.
const char* const WebUIContentsPreloadManager::kPreloadedWebUIURL =
    chrome::kChromeUITabSearchURL;

WebUIContentsPreloadManager::WebUIContentsPreloadManager() = default;
WebUIContentsPreloadManager::~WebUIContentsPreloadManager() = default;

// static
WebUIContentsPreloadManager* WebUIContentsPreloadManager::GetInstance() {
  static base::NoDestructor<WebUIContentsPreloadManager> s_instance;
  // Ensure that the shutdown notifier factory is initialized.
  // The profile service's dependency manager requires the service factory
  // be registered at an early stage of browser lifetime.
  BrowserContextShutdownNofifierFactory::GetInstance();
  return s_instance.get();
}

void WebUIContentsPreloadManager::PreloadForBrowserContext(
    content::BrowserContext* browser_context) {
  if (!ShouldPreloadForBrowserContext(browser_context)) {
    return;
  }

  preloaded_web_contents_ = CreateNewContents(browser_context);
}

std::unique_ptr<content::WebContents> WebUIContentsPreloadManager::MakeContents(
    const GURL& webui_url,
    content::BrowserContext* browser_context) {
  std::unique_ptr<content::WebContents> web_contents_ret;
  if (!preloaded_web_contents_ ||
      preloaded_web_contents_->GetBrowserContext() != browser_context) {
    // No preloaded contents, or the preloaded contents is under a different
    // context.
    web_contents_ret = CreateNewContents(browser_context, webui_url);
  } else {
    if (preloaded_web_contents_->GetURL().host() != webui_url.host()) {
      // Redirect if the preloaded contents is on a different WebUI.
      preloaded_web_contents_->GetController().LoadURL(
          webui_url, content::Referrer(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
          std::string());
    }
    web_contents_ret = std::move(preloaded_web_contents_);
  }

  if (ShouldPreloadForBrowserContext(browser_context)) {
    // Preloads a new contents.
    preloaded_web_contents_ = CreateNewContents(browser_context);
  }

  return web_contents_ret;
}

GURL WebUIContentsPreloadManager::GetPreloadedURLForTesting() const {
  return GURL(kPreloadedWebUIURL);
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

  web_contents->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());

  // Cleans up the preloaded contents on browser context shutdown.
  browser_context_shutdown_subscription_ =
      BrowserContextShutdownNofifierFactory::GetInstance()
          ->Get(browser_context)
          ->Subscribe(base::BindRepeating(
              &WebUIContentsPreloadManager::OnBrowserContextShutdown,
              base::Unretained(this), browser_context));

  return web_contents;
}

bool WebUIContentsPreloadManager::ShouldPreloadForBrowserContext(
    content::BrowserContext* browser_context) const {
  // Don't preload if the feature is disabled.
  if (!base::FeatureList::IsEnabled(features::kPreloadTopChromeWebUI)) {
    return false;
  }

  // Don't preload if already preloaded for this `browser_context`.
  if (preloaded_web_contents_ &&
      preloaded_web_contents_->GetBrowserContext() == browser_context) {
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

void WebUIContentsPreloadManager::OnBrowserContextShutdown(
    content::BrowserContext* browser_context) {
  if (!preloaded_web_contents_) {
    return;
  }

  CHECK_EQ(preloaded_web_contents_->GetBrowserContext(), browser_context);
  preloaded_web_contents_.reset();
}
