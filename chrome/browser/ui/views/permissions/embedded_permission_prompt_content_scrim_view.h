// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_CONTENT_SCRIM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_CONTENT_SCRIM_VIEW_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_view_delegate.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

// The view class, which is owned by the `EmbeddedPermissionPrompt`, applies a
// scrim to the contents view on the root window while the permission prompt is
// presenting. This view will always float above and cover entirely the root
// window's content view, the intention is to obscure the content UI and prevent
// web-content interaction while the user is interacting with the permission
// prompt.
// The `EmbeddedPermissionPrompt` is responsible for managing the lifetime of
// the scrim view and the in-progress `EmbeddedPermissionPromptBaseView`. When
// the prompt view is destroyed, the scrim is automatically removed. Clicking on
// this scrim view will destroy both the scrim view and the in-progress
// `EmbeddedPermissionPromptBaseView`.
class EmbeddedPermissionPromptContentScrimView : public views::View,
                                                 public views::WidgetObserver {
  METADATA_HEADER(EmbeddedPermissionPromptContentScrimView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kContentScrimViewId);

  class Delegate {
   public:
    virtual void DismissScrim() = 0;
    virtual base::WeakPtr<permissions::PermissionPrompt::Delegate>
    GetPermissionPromptDelegate() const = 0;
  };

  EmbeddedPermissionPromptContentScrimView(base::WeakPtr<Delegate> delegate,
                                           views::Widget* widget);

  ~EmbeddedPermissionPromptContentScrimView() override;

  EmbeddedPermissionPromptContentScrimView(
      const EmbeddedPermissionPromptContentScrimView&) = delete;
  EmbeddedPermissionPromptContentScrimView& operator=(
      const EmbeddedPermissionPromptContentScrimView&) = delete;

  // Create and returns the widget that contains this scrim view.
  static std::unique_ptr<views::Widget> CreateScrimWidget(
      base::WeakPtr<Delegate> delegate,
      SkColor color);

  // Views::View
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::WidgetObserver
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  base::ScopedObservation<views::Widget, WidgetObserver> observation_{this};

  base::WeakPtr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_CONTENT_SCRIM_VIEW_H_
