// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_controller_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_config.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/video_tutorials/video_player_ui.h"
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/web_applications/terminal_ui.h"
#if !defined(OFFICIAL_BUILD)
#include "chromeos/components/sample_system_web_app_ui/untrusted_sample_system_web_app_ui.h"
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using WebUIConfigList =
    std::vector<std::pair<std::string, std::unique_ptr<ui::WebUIConfig>>>;

namespace {

// Returns a std::vector<> containing all WebUIConfigs. We use a vector instead
// of adding WebUIConfigs directly into the flat_map because individual inserts
// are O(n), giving O(n^2) construction time for the entire map. By contrast,
// constructing from a vector is O(n log n).
WebUIConfigList CreateConfigs() {
  WebUIConfigList config_list;
  auto register_config =
      [&config_list](std::unique_ptr<ui::WebUIConfig> config) {
        DCHECK_EQ(config->scheme(), content::kChromeUIUntrustedScheme);
        const std::string& host = config->host();
        config_list.emplace_back(host, std::move(config));
      };
  // Delete once register_config is used outside of Chrome OS.
  ALLOW_UNUSED_LOCAL(register_config);

  // Register WebUIConfigs below.
#if defined(OS_ANDROID)
  register_config(std::make_unique<video_tutorials::VideoPlayerUIConfig>());
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  register_config(std::make_unique<TerminalUIConfig>());
#if !defined(OFFICIAL_BUILD)
  register_config(
      std::make_unique<chromeos::UntrustedSampleSystemWebAppUIConfig>());
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return config_list;
}

}  // namespace

// static
void ChromeUntrustedWebUIControllerFactory::RegisterInstance() {
  static base::NoDestructor<ChromeUntrustedWebUIControllerFactory> instance;
  content::WebUIControllerFactory::RegisterFactory(instance.get());
}

ChromeUntrustedWebUIControllerFactory::ChromeUntrustedWebUIControllerFactory()
    : configs_(CreateConfigs()) {}

ChromeUntrustedWebUIControllerFactory::
    ~ChromeUntrustedWebUIControllerFactory() = default;

const ui::UntrustedWebUIControllerFactory::WebUIConfigMap&
ChromeUntrustedWebUIControllerFactory::GetWebUIConfigMap() {
  return configs_;
}
