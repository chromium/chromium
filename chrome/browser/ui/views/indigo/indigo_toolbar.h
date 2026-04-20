// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INDIGO_INDIGO_TOOLBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INDIGO_INDIGO_TOOLBAR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

namespace views {
class ToggleImageButton;
class View;
}  // namespace views

namespace indigo {

// Owns and manages a toolbar widget (and its contents) for Indigo. This widget
// is collapsible (using an expand/collapse control) and closable, in addition
// to offering buttons for controlling the Indigo feature itself.
//
// It is intended to be a child of the WebContents' native view, and draw on top
// of the web page which is being modified.
class IndigoToolbar {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToolbarElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExpandButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRegenerateButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kReplacePhotoButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDeletePhotoButtonElementId);

  class Delegate {
   public:
    // Called when the toolbar is closed. Note that this can happen during the
    // IndigoToolbar destructor, and the underlying widget has been destroyed
    // by this point. Delegate implementations may want to destroy the
    // underlying IndigoToolbar object in response.
    virtual void OnClose(IndigoToolbar* toolbar) = 0;

    virtual void OnRegenerate(IndigoToolbar* toolbar) = 0;
    virtual void OnReplaceOriginalPhoto(IndigoToolbar* toolbar) = 0;
    virtual void OnDeleteOriginalPhoto(IndigoToolbar* toolbar) = 0;
  };

  explicit IndigoToolbar(Delegate* delegate);
  IndigoToolbar(const IndigoToolbar&) = delete;
  IndigoToolbar& operator=(const IndigoToolbar&) = delete;
  ~IndigoToolbar();

  void Show(gfx::NativeView parent_view);

  // Shows the toolbar at a position computed by the given callback.
  // The callback `toolbar_origin_func` receives the preferred size of the
  // toolbar view, excluding the view shadow, and should return the origin point
  // of the content area (also excluding the view shadow, relative to the web
  // view) where the toolbar should be placed. This is useful when the
  // positioning depends on the toolbar's preferred size, which is only known
  // after layout.
  void ShowAt(
      gfx::NativeView parent_view,
      base::FunctionRef<gfx::Point(const gfx::Size&)> toolbar_origin_func);

  void ShowInside(gfx::NativeView parent_view, const gfx::Rect& rect);
  void Hide();

 private:
  std::unique_ptr<views::View> CreateToolbarView();
  std::unique_ptr<views::Button> CreateExpandedButton(
      const std::u16string& label,
      const gfx::VectorIcon& icon,
      views::Button::PressedCallback callback);

  void OnCloseButtonClicked();
  void OnExpandButtonClicked();
  void OnRegenerateButtonClicked();
  void OnReplacePhotoClicked();
  void OnDeletePhotoClicked();

  void OnWidgetClosed(views::Widget::ClosedReason reason);

  raw_ptr<Delegate> delegate_;
  std::unique_ptr<views::Widget> widget_;

  bool is_expanded_ = false;
  raw_ptr<views::View> expanded_container_ = nullptr;
  raw_ptr<views::ToggleImageButton> expand_button_ = nullptr;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_UI_VIEWS_INDIGO_INDIGO_TOOLBAR_H_
