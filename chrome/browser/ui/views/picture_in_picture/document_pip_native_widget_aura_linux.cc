// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_native_widget_aura_linux.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_aura_linux.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view_linux.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/linux/device_scale_factor_observer.h"
#include "ui/linux/linux_ui.h"
#include "ui/native_theme/native_theme.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_view_utils_linux.h"
#include "ui/views/window/non_client_view.h"

namespace {

class DocumentPipWidgetLinux : public views::Widget {
 public:
  explicit DocumentPipWidgetLinux(Profile* profile)
      : profile_(CHECK_DEREF(profile)) {}

  DocumentPipWidgetLinux(const DocumentPipWidgetLinux&) = delete;
  DocumentPipWidgetLinux& operator=(const DocumentPipWidgetLinux&) = delete;

  const ui::NativeTheme* GetNativeTheme() const override {
    // Let Widget::Init() establish the native-theme observation.
    auto* linux_ui_theme =
        ThemeServiceAuraLinux::GetLinuxUiThemeForProfile(&profile_.get());
    return linux_ui_theme && !profile_->IsIncognitoProfile() &&
                   !profile_->IsEnterpriseIsolatedModeProfile()
               ? linux_ui_theme->GetNativeTheme()
               : ui::NativeTheme::GetInstanceForNativeUi();
  }

  const ui::ThemeProvider* GetThemeProvider() const override {
    return &ThemeService::GetThemeProviderForProfile(&profile_.get());
  }

  ui::ColorProviderKey::ThemeInitializerSupplier* GetCustomTheme()
      const override {
    if (profile_->IsIncognitoProfile() ||
        profile_->IsEnterpriseIsolatedModeProfile() ||
        user_color_override().has_value()) {
      return nullptr;
    }
    auto* theme_service = ThemeServiceFactory::GetForProfile(&profile_.get());
    return theme_service->UsingDeviceTheme()
               ? nullptr
               : theme_service->GetThemeSupplier();
  }

 private:
  // The profile outlives the widget, including opener/dialog teardown.
  const raw_ref<Profile> profile_;
};

class DocumentPipDesktopWindowTreeHostLinux
    : public views::DesktopWindowTreeHostLinux,
      public ui::DeviceScaleFactorObserver {
 public:
  DocumentPipDesktopWindowTreeHostLinux(
      views::Widget* widget,
      views::DesktopNativeWidgetAura* native_widget)
      : views::DesktopWindowTreeHostLinux(widget, native_widget) {
    if (auto* linux_ui = ui::LinuxUi::instance()) {
      scale_observation_.Observe(linux_ui);
    }
  }

  DocumentPipDesktopWindowTreeHostLinux(
      const DocumentPipDesktopWindowTreeHostLinux&) = delete;
  DocumentPipDesktopWindowTreeHostLinux& operator=(
      const DocumentPipDesktopWindowTreeHostLinux&) = delete;

  void OnWidgetInitDone() override {
    views::DesktopWindowTreeHostLinux::OnWidgetInitDone();
    UpdateFrameHints();
  }

  void UpdateFrameHints() override {
    const auto* frame = GetPipFrame();
    if (!frame) {
      return;
    }
    auto* window = platform_window();
    const float scale = device_scale_factor();
    const gfx::Size size = frame->GetWidget()->GetWindowBoundsInScreen().size();
    const auto state = window->GetPlatformWindowState();
    const gfx::Insets insets = CalculateInsetsInDIP(state);

    if (SupportsClientFrameShadow()) {
      if (insets.IsEmpty()) {
        window->SetInputRegion(std::nullopt);
      } else {
        gfx::Rect input_bounds(size);
        input_bounds.Inset(insets - frame->GetInputInsets());
        window->SetInputRegion(std::vector<gfx::Rect>(
            {gfx::ScaleToEnclosingRect(input_bounds, scale)}));
      }
    }

    if (ui::OzonePlatform::GetInstance()->IsWindowCompositingSupported()) {
      window->SetOpaqueRegion(
          IsShowingFrame(state)
              ? views::GetRestoredOpaqueRegion(frame->GetRestoredClipRegion(),
                                               scale, 0)
              : std::vector<gfx::Rect>(
                    {gfx::ScaleToEnclosingRect(gfx::Rect(size), scale)}));
    }
    SizeConstraintsChanged();
  }

 protected:
  void AddAdditionalInitProperties(
      const views::Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override {
    views::DesktopWindowTreeHostLinux::AddAdditionalInitProperties(params,
                                                                   properties);
    auto* linux_ui_theme = ui::LinuxUiTheme::GetForWindow(GetContentWindow());
    properties->prefer_dark_theme =
        linux_ui_theme && linux_ui_theme->PreferDarkTheme();
  }

  gfx::Insets CalculateInsetsInDIP(
      ui::PlatformWindowState window_state) const override {
    const auto* frame = GetPipFrame();
    return frame && IsShowingFrame(window_state)
               ? frame->RestoredMirroredFrameBorderInsets()
               : gfx::Insets();
  }

  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override {
    views::DesktopWindowTreeHostLinux::OnWindowStateChanged(old_state,
                                                            new_state);
    UpdateFrameHints();
  }

  void OnWindowTiledStateChanged(ui::WindowTiledEdges edges) override {
    if (auto* frame = GetPipFrame()) {
      frame->SetTiled(edges.top || edges.left || edges.bottom || edges.right);
      UpdateFrameHints();
    }
  }

  void OnNativeThemeUpdated(ui::NativeTheme* theme) override {
    views::DesktopWindowTreeHostLinux::OnNativeThemeUpdated(theme);
    UpdateFrameHints();
  }

 private:
  static bool IsShowingFrame(ui::PlatformWindowState state) {
    return state != ui::PlatformWindowState::kMaximized &&
           state != ui::PlatformWindowState::kFullScreen &&
           state != ui::PlatformWindowState::kMinimized;
  }

  bool SupportsClientFrameShadow() const {
    return platform_window()->CanSetDecorationInsets() &&
           views::Widget::IsWindowCompositingSupported();
  }

  DocumentPipFrameViewLinux* GetPipFrame() const {
    // Native events can arrive after the Widget and its DocumentPipHost die.
    const auto* widget = GetWidget();
    return widget && widget->non_client_view()
               ? views::AsViewClass<DocumentPipFrameViewLinux>(
                     widget->non_client_view()->frame_view())
               : nullptr;
  }

  // ui::DeviceScaleFactorObserver:
  void OnDeviceScaleFactorChanged() override { UpdateFrameHints(); }

  base::ScopedObservation<ui::LinuxUi, ui::DeviceScaleFactorObserver>
      scale_observation_{this};
};

}  // namespace

std::unique_ptr<views::Widget> CreateDocumentPipWidgetLinux(
    DocumentPipHost* host,
    views::Widget::InitParams& params) {
  auto widget = std::make_unique<DocumentPipWidgetLinux>(host->GetProfile());
  auto* native_widget = new views::DesktopNativeWidgetAura(widget.get());
  SetThemeProfileForWindow(native_widget->GetNativeWindow(),
                           host->GetProfile()->GetOriginalProfile());

  params.wm_class_name = shell_integration_linux::GetProgramClassName();
  params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  params.wm_role_name = "pop-up";
  params.wayland_app_id = params.wm_class_class;
  const std::string desktop_id =
      shell_integration_linux::GetInstalledPerProfileDesktopId(
          host->GetProfile()->GetOriginalProfile()->GetPath());
  if (!desktop_id.empty()) {
    params.wm_class_name = desktop_id;
    params.wayland_app_id = desktop_id;
  }
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.native_widget = native_widget;
  params.desktop_window_tree_host =
      new DocumentPipDesktopWindowTreeHostLinux(widget.get(), native_widget);
  return widget;
}
