// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_GTK_UI_H_
#define CHROME_BROWSER_UI_LIBGTKUI_GTK_UI_H_

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "build/buildflag.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/window/frame_buttons.h"

typedef struct _GParamSpec GParamSpec;
typedef struct _GtkParamSpec GtkParamSpec;
typedef struct _GtkSettings GtkSettings;
typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;

namespace libgtkui {
using ColorMap = std::map<int, SkColor>;

class GtkKeyBindingsHandler;
class DeviceScaleFactorObserver;
class NativeThemeGtk;
class SettingsProvider;

// Interface to GTK desktop features.
class GtkUi : public views::LinuxUI {
 public:
  GtkUi();
  ~GtkUi() override;

  // Setters used by SettingsProvider:
  void SetWindowButtonOrdering(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons);
  void SetWindowFrameAction(WindowFrameActionSource source,
                            WindowFrameAction action);

  // ui::LinuxInputMethodContextFactory:
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const override;

  // gfx::LinuxFontDelegate:
  gfx::FontRenderParams GetDefaultFontRenderParams() const override;
  void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      gfx::Font::Weight* weight_out,
      gfx::FontRenderParams* params_out) const override;

  // ui::ShellDialogLinux:
  ui::SelectFileDialog* CreateSelectFileDialog(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) const override;

  // views::LinuxUI:
  void Initialize() override;
  bool GetTint(int id, color_utils::HSL* tint) const override;
  bool GetColor(int id,
                SkColor* color,
                PrefService* pref_service) const override;
  bool GetDisplayProperty(int id, int* result) const override;
  SkColor GetFocusRingColor() const override;
  SkColor GetActiveSelectionBgColor() const override;
  SkColor GetActiveSelectionFgColor() const override;
  SkColor GetInactiveSelectionBgColor() const override;
  SkColor GetInactiveSelectionFgColor() const override;
  base::TimeDelta GetCursorBlinkInterval() const override;
  ui::NativeTheme* GetNativeTheme(aura::Window* window) const override;
  void SetUseSystemThemeCallback(UseSystemThemeCallback callback) override;
  bool GetDefaultUsesSystemTheme() const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size) const override;
  std::unique_ptr<views::Border> CreateNativeBorder(
      views::LabelButton* owning_button,
      std::unique_ptr<views::LabelButtonBorder> border) override;
  void AddWindowButtonOrderObserver(
      views::WindowButtonOrderObserver* observer) override;
  void RemoveWindowButtonOrderObserver(
      views::WindowButtonOrderObserver* observer) override;
  WindowFrameAction GetWindowFrameAction(
      WindowFrameActionSource source) override;
  void NotifyWindowManagerStartupComplete() override;
  void UpdateDeviceScaleFactor() override;
  float GetDeviceScaleFactor() const override;
  void AddDeviceScaleFactorObserver(
      views::DeviceScaleFactorObserver* observer) override;
  void RemoveDeviceScaleFactorObserver(
      views::DeviceScaleFactorObserver* observer) override;
  bool PreferDarkTheme() const override;
#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
  std::unique_ptr<views::NavButtonProvider> CreateNavButtonProvider() override;
#endif
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  std::string GetCursorThemeName() override;
  int GetCursorThemeSize() override;

  // ui::TextEditKeybindingDelegate:
  bool MatchEvent(const ui::Event& event,
                  std::vector<ui::TextEditCommandAuraLinux>* commands) override;

 private:
  using TintMap = std::map<int, color_utils::HSL>;

  CHROMEG_CALLBACK_1(GtkUi, void, OnThemeChanged, GtkSettings*, GtkParamSpec*);

  CHROMEG_CALLBACK_1(GtkUi,
                     void,
                     OnCursorThemeNameChanged,
                     GtkSettings*,
                     GtkParamSpec*);

  CHROMEG_CALLBACK_1(GtkUi,
                     void,
                     OnCursorThemeSizeChanged,
                     GtkSettings*,
                     GtkParamSpec*);

  CHROMEG_CALLBACK_1(GtkUi,
                     void,
                     OnDeviceScaleFactorMaybeChanged,
                     void*,
                     GParamSpec*);

  // Loads all GTK-provided settings.
  void LoadGtkValues();

  // Extracts colors and tints from the GTK theme, both for the
  // ThemeService interface and the colors we send to Blink.
  void UpdateColors();

  // Updates |default_font_*|.
  void UpdateDefaultFont();

  float GetRawDeviceScaleFactor();

  NativeThemeGtk* native_theme_;

  // A regular GtkWindow.
  GtkWidget* fake_window_;

  // Colors calculated by LoadGtkValues() that are given to the
  // caller while |use_gtk_| is true.
  ColorMap colors_;

  // Frame colors (and colors that depend on frame colors) when using
  // Chrome-rendered borders and titlebar.
  ColorMap custom_frame_colors_;

  // Frame colors (and colors that depend on frame colors) when using
  // system-rendered borders and titlebar.
  ColorMap native_frame_colors_;

  // Colors that we pass to Blink. These are generated each time the theme
  // changes.
  SkColor focus_ring_color_;
  SkColor active_selection_bg_color_;
  SkColor active_selection_fg_color_;
  SkColor inactive_selection_bg_color_;
  SkColor inactive_selection_fg_color_;

  // Details about the default UI font.
  std::string default_font_family_;
  int default_font_size_pixels_ = 0;
  // Bitfield of gfx::Font::Style values.
  int default_font_style_ = gfx::Font::NORMAL;
  gfx::Font::Weight default_font_weight_ = gfx::Font::Weight::NORMAL;
  gfx::FontRenderParams default_font_render_params_;

  std::unique_ptr<SettingsProvider> settings_provider_;

  // Frame button layout state.  If |nav_buttons_set_| is false, then
  // |leading_buttons_| and |trailing_buttons_| are meaningless.
  bool nav_buttons_set_ = false;
  std::vector<views::FrameButton> leading_buttons_;
  std::vector<views::FrameButton> trailing_buttons_;

  std::unique_ptr<GtkKeyBindingsHandler> key_bindings_handler_;

  // Objects to notify when the window frame button order changes.
  base::ObserverList<views::WindowButtonOrderObserver>::Unchecked
      window_button_order_observer_list_;

  // Objects to notify when the device scale factor changes.
  base::ObserverList<views::DeviceScaleFactorObserver>::Unchecked
      device_scale_factor_observer_list_;

  // The action to take when middle, double, or right clicking the titlebar.
  base::flat_map<WindowFrameActionSource, WindowFrameAction>
      window_frame_actions_;

  // Used to determine whether the system theme should be used for a window.  If
  // no override is provided or the callback returns true, GtkUi will default
  // to a NativeThemeGtk instance.
  UseSystemThemeCallback use_system_theme_callback_;

  float device_scale_factor_ = 1.0f;

  DISALLOW_COPY_AND_ASSIGN(GtkUi);
};

}  // namespace libgtkui

// Access point to the GTK desktop system.
COMPONENT_EXPORT(LIBGTKUI) views::LinuxUI* BuildGtkUi();

#endif  // CHROME_BROWSER_UI_LIBGTKUI_GTK_UI_H_
