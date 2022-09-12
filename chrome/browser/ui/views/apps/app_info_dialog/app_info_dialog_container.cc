// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_container.h"

#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/views/background.h"
#endif

namespace {

#if BUILDFLAG(IS_MAC)
const ui::ModalType kModalType = ui::MODAL_TYPE_CHILD;
const views::BubbleBorder::Shadow kShadowType = views::BubbleBorder::NO_SHADOW;
#else
const ui::ModalType kModalType = ui::MODAL_TYPE_WINDOW;
const views::BubbleBorder::Shadow kShadowType =
    views::BubbleBorder::STANDARD_SHADOW;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The background for App List dialogs, which appears as a rounded rectangle
// with the same border radius and color as the app list contents.
class AppListOverlayBackground : public views::Background {
 public:
  AppListOverlayBackground() = default;
  AppListOverlayBackground(const AppListOverlayBackground&) = delete;
  AppListOverlayBackground& operator=(const AppListOverlayBackground&) = delete;
  ~AppListOverlayBackground() override = default;

  // Overridden from views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    // The radius of the app list overlay (the dialog's background).
    // TODO(sashab): Using SupportsShadow() from app_list_view.cc, make this
    // 1px smaller on platforms that support shadows.
    const int kAppListOverlayBorderRadius = 3;

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(ash::AppListColorProvider::Get()->GetContentsBackgroundColor(
        view->GetWidget()));
    canvas->DrawRoundRect(view->GetContentsBounds(),
                          kAppListOverlayBorderRadius, flags);
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A BubbleFrameView that allows its client view to extend all the way to the
// top of the dialog, overlapping the BubbleFrameView's close button. This
// allows dialog content to appear closer to the top, in place of a title.
// TODO(estade): the functionality here should probably be folded into
// BubbleFrameView.
class FullSizeBubbleFrameView : public views::BubbleFrameView {
 public:
  METADATA_HEADER(FullSizeBubbleFrameView);
  FullSizeBubbleFrameView()
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()) {}
  FullSizeBubbleFrameView(const FullSizeBubbleFrameView&) = delete;
  FullSizeBubbleFrameView& operator=(const FullSizeBubbleFrameView&) = delete;
  ~FullSizeBubbleFrameView() override = default;

 private:
  // Overridden from views::BubbleFrameView:
  bool ExtendClientIntoTitle() const override { return true; }
};

BEGIN_METADATA(FullSizeBubbleFrameView, views::BubbleFrameView)
END_METADATA

// A container view for a native dialog, which sizes to the given fixed |size|.
class NativeDialogContainer : public views::DialogDelegateView {
 public:
  METADATA_HEADER(NativeDialogContainer);
  NativeDialogContainer(std::unique_ptr<views::View> dialog_body,
                        const gfx::Size& size,
                        base::OnceClosure close_callback) {
    SetButtons(ui::DIALOG_BUTTON_NONE);
    SetModalType(kModalType);
    AddChildView(std::move(dialog_body));
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetPreferredSize(size);

    if (!close_callback.is_null()) {
      RegisterWindowClosingCallback(std::move(close_callback));
    }
  }
  NativeDialogContainer(const NativeDialogContainer&) = delete;
  NativeDialogContainer& operator=(const NativeDialogContainer&) = delete;
  ~NativeDialogContainer() override = default;

 private:
  // Overridden from views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    auto frame = std::make_unique<FullSizeBubbleFrameView>();
    frame->SetBubbleBorder(std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, kShadowType));
    return frame;
  }
};

BEGIN_METADATA(NativeDialogContainer, views::DialogDelegateView)
END_METADATA

}  // namespace

views::DialogDelegateView* CreateDialogContainerForView(
    std::unique_ptr<views::View> view,
    const gfx::Size& size,
    base::OnceClosure close_callback) {
  return new NativeDialogContainer(std::move(view), size,
                                   std::move(close_callback));
}
