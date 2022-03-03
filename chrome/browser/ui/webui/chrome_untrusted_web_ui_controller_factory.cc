// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_controller_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "printing/buildflags/buildflags.h"
#include "ui/webui/webui_config.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui_untrusted.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/video_tutorials/video_player_ui.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/feed/feed_ui_config.h"
#include "chrome/browser/ui/webui/image_editor/image_editor_untrusted_ui.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/untrusted_eche_app_ui.h"
#include "ash/webui/file_manager/file_manager_untrusted_ui.h"
#include "ash/webui/help_app_ui/help_app_kids_magazine_untrusted_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_untrusted_ui.h"
#include "chrome/browser/ash/web_applications/camera_app/camera_app_untrusted_ui_config.h"
#include "chrome/browser/ash/web_applications/crosh_ui.h"
#include "chrome/browser/ash/web_applications/help_app/help_app_untrusted_ui_config.h"
#include "chrome/browser/ash/web_applications/media_app/media_app_guest_ui_config.h"
#include "chrome/browser/ash/web_applications/projector_app/untrusted_projector_annotator_ui_config.h"
#include "chrome/browser/ash/web_applications/projector_app/untrusted_projector_ui_config.h"
#include "chrome/browser/ash/web_applications/terminal_ui.h"
#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/untrusted_sample_system_web_app_ui.h"
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
  // Delete [[maybe_unused]] once register_config is used outside of Chrome OS.
  [[maybe_unused]] auto register_config =
      [&config_list](std::unique_ptr<ui::WebUIConfig> config) {
        DCHECK_EQ(config->scheme(), content::kChromeUIUntrustedScheme);
        const std::string& host = config->host();
        config_list.emplace_back(host, std::move(config));
      };

  // Register WebUIConfigs below.
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  register_config(std::make_unique<printing::PrintPreviewUIUntrustedConfig>());
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_ANDROID)
  register_config(std::make_unique<video_tutorials::VideoPlayerUIConfig>());
#else  // !BUILDFLAG(IS_ANDROID)
  register_config(std::make_unique<feed::FeedUIConfig>());
  register_config(
      std::make_unique<image_editor::ImageEditorUntrustedUIConfig>());
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  register_config(std::make_unique<CroshUIConfig>());
  register_config(std::make_unique<TerminalUIConfig>());
  register_config(std::make_unique<ash::eche_app::UntrustedEcheAppUIConfig>());
  register_config(std::make_unique<MediaAppGuestUIConfig>());
  register_config(std::make_unique<ash::HelpAppUntrustedUIConfig>());
  register_config(std::make_unique<ash::CameraAppUntrustedUIConfig>());
  register_config(
      std::make_unique<ash::HelpAppKidsMagazineUntrustedUIConfig>());
  if (ash::features::IsProjectorEnabled())
    register_config(std::make_unique<UntrustedProjectorUIConfig>());
  if (ash::features::IsProjectorAnnotatorEnabled())
    register_config(std::make_unique<UntrustedProjectorAnnotatorUIConfig>());
  if (ash::features::IsFileManagerSwaEnabled()) {
    register_config(
        std::make_unique<ash::file_manager::FileManagerUntrustedUIConfig>());
  }
  if (base::FeatureList::IsEnabled(ash::features::kOsFeedback)) {
    register_config(
        std::make_unique<ash::feedback::OsFeedbackUntrustedUIConfig>());
  }
#if !defined(OFFICIAL_BUILD)
  register_config(std::make_unique<ash::UntrustedSampleSystemWebAppUIConfig>());
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return config_list;
}

}  // namespace

// static
ChromeUntrustedWebUIControllerFactory*
ChromeUntrustedWebUIControllerFactory::GetInstance() {
  static base::NoDestructor<ChromeUntrustedWebUIControllerFactory> instance;
  return instance.get();
}

// static
void ChromeUntrustedWebUIControllerFactory::RegisterInstance() {
  content::WebUIControllerFactory::RegisterFactory(GetInstance());
}

ChromeUntrustedWebUIControllerFactory::ChromeUntrustedWebUIControllerFactory()
    : configs_(CreateConfigs()) {}

ChromeUntrustedWebUIControllerFactory::
    ~ChromeUntrustedWebUIControllerFactory() = default;

const ui::UntrustedWebUIControllerFactory::WebUIConfigMap&
ChromeUntrustedWebUIControllerFactory::GetWebUIConfigMap() {
  return configs_;
}
