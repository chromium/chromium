// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BASE_SHIM_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BASE_SHIM_H_

#include <type_traits>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"

#define DEFINE_MIXIN(METHOD_NAME, FULL_SIGNATURE, ...)                      \
  template <typename T, typename Void, typename... Args>                    \
  struct Has##METHOD_NAME##_Internal : std::false_type {};                  \
                                                                            \
  template <typename T, typename... Args>                                   \
  struct Has##METHOD_NAME##_Internal<                                       \
      T,                                                                    \
      std::void_t<decltype(std::declval<T>().METHOD_NAME(                   \
          std::declval<Args>()...))>,                                       \
      Args...> : std::true_type {};                                         \
                                                                            \
  template <typename T>                                                     \
  struct Has##METHOD_NAME                                                   \
      : Has##METHOD_NAME##_Internal<T, void __VA_OPT__(, ) __VA_ARGS__> {}; \
                                                                            \
  template <typename T, bool HasMethod = Has##METHOD_NAME<T>::value>        \
  class METHOD_NAME##Mixin : public T {                                     \
   public:                                                                  \
    using T::T;                                                             \
  };                                                                        \
                                                                            \
  template <typename T>                                                     \
  class METHOD_NAME##Mixin<T, false> : public T {                           \
   public:                                                                  \
    using T::T;                                                             \
    virtual FULL_SIGNATURE                                                  \
  };

namespace glic {

// Define Mixins that will either define a no-op virtual method to be overridden
// or use the template class's method.
DEFINE_MIXIN(UpdateColors, void UpdateColors(){})
DEFINE_MIXIN(
    GetIsShowingNudge,
    bool GetIsShowingNudge() const { return false; })
DEFINE_MIXIN(UpdateIcon, void UpdateIcon(){})
DEFINE_MIXIN(
    ShowContextMenuForViewImpl,
    void ShowContextMenuForViewImpl(views::View* source,
                                    const gfx::Point& point,
                                    ui::mojom::MenuSourceType source_type){},
    views::View*,
    const gfx::Point&,
    ui::mojom::MenuSourceType)
DEFINE_MIXIN(SetWidthFactor, void SetWidthFactor(float factor){}, float)

// GlicBaseShim is used by Glic views that are required to be multiple types of
// buttons (e.g. TabStripNudgeButton and ToolbarButton). GlicBaseShim provides
// an interface that allows for certain methods either be implemented by T, or
// overridden by the subclass. These methods are often required for unified GLic
// functionality, but are not necessary for all T views.
template <typename T>
class GlicBaseShim
    : public SetWidthFactorMixin<ShowContextMenuForViewImplMixin<
          UpdateIconMixin<GetIsShowingNudgeMixin<UpdateColorsMixin<T>>>>>,
      public GlicButtonInterface {
 public:
  using SetWidthFactorMixin<ShowContextMenuForViewImplMixin<UpdateIconMixin<
      GetIsShowingNudgeMixin<UpdateColorsMixin<T>>>>>::SetWidthFactorMixin;

  virtual void SetCloseButtonFocusBehavior(
      views::View::FocusBehavior focus_behavior) {
    if constexpr (requires {
                    this->T::SetCloseButtonFocusBehavior(focus_behavior);
                  }) {
      T::SetCloseButtonFocusBehavior(focus_behavior);
    }
  }

  virtual void SetForegroundFrameActiveColorId(ui::ColorId new_color_id) {
    if constexpr (requires {
                    this->T::SetForegroundFrameActiveColorId(new_color_id);
                  }) {
      T::SetForegroundFrameActiveColorId(new_color_id);
    }
  }

  virtual void SetForegroundFrameInactiveColorId(ui::ColorId new_color_id) {
    if constexpr (requires {
                    this->T::SetForegroundFrameInactiveColorId(new_color_id);
                  }) {
      T::SetForegroundFrameInactiveColorId(new_color_id);
    }
  }

  virtual void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id) {
    if constexpr (requires {
                    this->T::SetBackgroundFrameActiveColorId(new_color_id);
                  }) {
      T::SetBackgroundFrameActiveColorId(new_color_id);
    }
  }

  virtual void SetBackgroundFrameInactiveColorId(ui::ColorId new_color_id) {
    if constexpr (requires {
                    this->T::SetBackgroundFrameInactiveColorId(new_color_id);
                  }) {
      T::SetBackgroundFrameInactiveColorId(new_color_id);
    }
  }

  virtual void SetLeftRightCornerRadii(int left, int right) {
    if constexpr (requires { this->T::SetLeftRightCornerRadii(left, right); }) {
      T::SetLeftRightCornerRadii(left, right);
    }
  }

  virtual void SetInkdropHoverColorId(const ChromeColorIds new_color_id) {
    if constexpr (requires { this->T::SetInkdropHoverColorId(new_color_id); }) {
      T::SetInkdropHoverColorId(new_color_id);
    }
  }

  void SetIsShowingNudge(bool is_showing) override {
    if constexpr (requires { this->T::SetIsShowingNudge(is_showing); }) {
      T::SetIsShowingNudge(is_showing);
    }
    is_showing_nudge_ = is_showing;
  }

  bool GetVisible() override { return T::GetVisible(); }

  ui::PropertyHandler* GetPropertyHandler() override { return this; }

 protected:
  // True if the button is showing a nudge.
  bool is_showing_nudge_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BASE_SHIM_H_
