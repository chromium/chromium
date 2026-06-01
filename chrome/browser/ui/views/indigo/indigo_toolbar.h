// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INDIGO_INDIGO_TOOLBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INDIGO_INDIGO_TOOLBAR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_tracker.h"

namespace views {
class View;
}  // namespace views

DECLARE_UI_CLASS_PROPERTY_TYPE(gfx::Point*)

namespace indigo {

extern const ui::ClassProperty<gfx::Point*>* const kIndigoToolbarOffsetKey;

std::unique_ptr<views::View> CreateIndigoOverlayView();

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
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExpandedContainerElementId);
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

  // `parent_view` should be the ContentsContainerView in practice (except in
  // unit tests), and this toolbar's view coordinates with it to arrange its
  // layout.
  void Show(views::View* parent_view);

  // Shows the toolbar at a position computed by the given callback.
  // The callback `toolbar_origin_func` receives the preferred size of the
  // toolbar view, excluding the view shadow, and should return the origin point
  // of the content area (also excluding the view shadow, relative to the web
  // view) where the toolbar should be placed. This is useful when the
  // positioning depends on the toolbar's preferred size, which is only known
  // after layout.
  //
  // `parent_view` should be the ContentsContainerView, as described above,
  // except in unit tests.
  void ShowAt(
      views::View* parent_view,
      base::FunctionRef<gfx::Point(const gfx::Size&)> toolbar_origin_func);

  void ShowInside(views::View* parent_view, const gfx::Rect& rect);
  void Hide();

  // Reclaims ownership of the toolbar view when the tab is hidden.
  void TabWillBecomeHidden();

  // Re-attaches the reclaimed view to the new parent view on tab visibility.
  void TabDidBecomeVisible(views::View* parent_view);

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

  raw_ptr<Delegate> delegate_;
  views::ViewTracker view_tracker_;

  // Holds the toolbar view when the tab is deactivated or not yet active,
  // preserving its state. Ownership is transferred back here in
  // TabWillDeactivate or during initial creation while inactive, and returned
  // to the parent view in ShowAt (called via TabDidActivate or Show).
  std::unique_ptr<views::View> owned_view_;

  bool is_expanded_ = false;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_UI_VIEWS_INDIGO_INDIGO_TOOLBAR_H_
