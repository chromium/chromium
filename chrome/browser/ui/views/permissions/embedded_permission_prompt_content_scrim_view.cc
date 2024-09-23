// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"

#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr char kWidgetName[] = "EmbeddedPermissionPromptContentScrimWidget";
}

EmbeddedPermissionPromptContentScrimView::
    EmbeddedPermissionPromptContentScrimView(base::WeakPtr<Delegate> delegate,
                                             views::Widget* widget)
    : delegate_(std::move(delegate)) {
  SetProperty(views::kElementIdentifierKey, kContentScrimViewId);
  observation_.Observe(widget);
}

EmbeddedPermissionPromptContentScrimView::
    ~EmbeddedPermissionPromptContentScrimView() = default;

// static
std::unique_ptr<views::Widget>
EmbeddedPermissionPromptContentScrimView::CreateScrimWidget(
    base::WeakPtr<Delegate> delegate,
    SkColor color) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  auto permission_prompt_delegate = delegate->GetPermissionPromptDelegate();
  CHECK(permission_prompt_delegate);
  auto* web_content = permission_prompt_delegate->GetAssociatedWebContents();
  auto* top_level_widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_content->GetContentNativeView());
  if (!top_level_widget) {
    return nullptr;
  }
  params.parent = top_level_widget->GetNativeView();
  params.bounds = web_content->GetContainerBounds();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = true;
  params.name = kWidgetName;
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));

  auto content_scrim_view =
      std::make_unique<EmbeddedPermissionPromptContentScrimView>(
          delegate, top_level_widget);
  content_scrim_view->SetBackground(views::CreateSolidBackground(color));
  widget->SetContentsView(std::move(content_scrim_view));
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->Show();
  return widget;
}

bool EmbeddedPermissionPromptContentScrimView::OnMousePressed(
    const ui::MouseEvent& event) {
  if (delegate_) {
    delegate_->DismissScrim();
  }
  return true;
}

void EmbeddedPermissionPromptContentScrimView::OnGestureEvent(
    ui::GestureEvent* event) {
  if (delegate_ && (event->type() == ui::EventType::kGestureTap ||
                    event->type() == ui::EventType::kGestureDoubleTap)) {
    delegate_->DismissScrim();
  }
}

void EmbeddedPermissionPromptContentScrimView::OnWidgetDestroyed(
    views::Widget* widget) {
  DCHECK(observation_.IsObservingSource(widget));
  observation_.Reset();
}

void EmbeddedPermissionPromptContentScrimView::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (!delegate_) {
    return;
  }
  if (auto permission_prompt_delegate =
          delegate_->GetPermissionPromptDelegate()) {
    GetWidget()->SetBounds(
        permission_prompt_delegate->GetAssociatedWebContents()
            ->GetContainerBounds());
  }
}

BEGIN_METADATA(EmbeddedPermissionPromptContentScrimView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EmbeddedPermissionPromptContentScrimView,
                                      kContentScrimViewId);
