// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/frame/browser_frame_view_win.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux_native.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_linux_native.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view_linux.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/nav_button_provider.h"
#endif

namespace chrome {

namespace {

#if BUILDFLAG(IS_LINUX)
std::unique_ptr<OpaqueBrowserFrameView> CreateOpaqueBrowserFrameViewLinux(
    BrowserFrame* frame,
    BrowserView* browser_view) {
  auto* profile = browser_view->browser()->profile();
  auto* linux_ui_theme = ui::LinuxUiTheme::GetForProfile(profile);
  auto* theme_service_factory = ThemeServiceFactory::GetForProfile(profile);
  auto* app_controller = browser_view->browser()->app_controller();

  // Ignore the toolkit theme for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073. Also ignore the toolkit theme for web apps with
  // borderless as there's no surface left to apply the theme for.
  bool app_uses_wco_or_borderless =
      app_controller && (app_controller->AppUsesWindowControlsOverlay() ||
                         app_controller->AppUsesBorderlessMode());

  if (linux_ui_theme && theme_service_factory->UsingSystemTheme() &&
      !app_uses_wco_or_borderless) {
    auto nav_button_provider = linux_ui_theme->CreateNavButtonProvider();
    if (nav_button_provider) {
      auto* native_frame = static_cast<DesktopBrowserFrameAuraLinux*>(
          frame->native_browser_frame());
      auto* layout = new BrowserFrameViewLayoutLinuxNative(
          nav_button_provider.get(),
          base::BindRepeating(
              [](DesktopBrowserFrameAuraLinux* native_frame,
                 ui::LinuxUiTheme* linux_ui_theme, bool tiled, bool maximized) {
                const bool solid_frame =
                    !native_frame->ShouldDrawRestoredFrameShadow();
                return linux_ui_theme->GetWindowFrameProvider(solid_frame,
                                                              tiled, maximized);
              },
              native_frame, linux_ui_theme));
      return std::make_unique<BrowserFrameViewLinuxNative>(
          frame, browser_view, layout, std::move(nav_button_provider));
    }
  }
  return std::make_unique<BrowserFrameViewLinux>(
      frame, browser_view, new BrowserFrameViewLayoutLinux());
}

std::unique_ptr<BrowserNonClientFrameView> CreateBrowserNonClientFrameViewLinux(
    BrowserFrame* frame,
    BrowserView* browser_view) {
  if (browser_view->browser()->is_type_picture_in_picture()) {
    return std::make_unique<PictureInPictureBrowserFrameViewLinux>(
        frame, browser_view);
  }

  auto opaque_browser_view =
      CreateOpaqueBrowserFrameViewLinux(frame, browser_view);
  opaque_browser_view->InitViews();

  return opaque_browser_view;
}
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
std::unique_ptr<BrowserNonClientFrameView> CreateBrowserNonClientFrameViewWin(
    BrowserFrame* frame,
    BrowserView* browser_view) {
  if (browser_view->browser()->is_type_picture_in_picture()) {
    return std::make_unique<PictureInPictureBrowserFrameView>(frame,
                                                              browser_view);
  }

  if (frame->ShouldUseNativeFrame()) {
    return std::make_unique<BrowserFrameViewWin>(frame, browser_view);
  }

  auto opaque_browser_view = std::make_unique<OpaqueBrowserFrameView>(
      frame, browser_view, new OpaqueBrowserFrameViewLayout());
  opaque_browser_view->InitViews();

  return opaque_browser_view;
}
#endif

}  // anonymous namespace

std::unique_ptr<BrowserNonClientFrameView> CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
#if BUILDFLAG(IS_WIN)
  return CreateBrowserNonClientFrameViewWin(frame, browser_view);
#else
  return CreateBrowserNonClientFrameViewLinux(frame, browser_view);
#endif
}

}  // namespace chrome
