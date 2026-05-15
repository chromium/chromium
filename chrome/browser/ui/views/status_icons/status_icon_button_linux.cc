// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_button_linux.h"

#include <limits>

#include "base/check.h"
#include "chrome/browser/shell_integration_linux.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/base/wm_role_names_linux.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"

namespace {

const int16_t kInitialWindowPos = std::numeric_limits<int16_t>::min();

class StatusIconWidget : public views::Widget {
 public:
  // xfce4-indicator-plugin requires a min size hint to be set on the window
  // (and it must be at least 2x2), otherwise it will not allocate any space to
  // the status icon window.
  gfx::Size GetMinimumSize() const override { return gfx::Size(2, 2); }
};

}  // namespace

class StatusIconButtonLinux::StatusIconHoverButton : public views::Button {
  METADATA_HEADER(StatusIconHoverButton, views::Button)

 public:
  explicit StatusIconHoverButton(StatusIconButtonLinux* host)
      : views::Button(base::BindRepeating(
            [](StatusIconButtonLinux* host) { host->delegate_->OnClick(); },
            base::Unretained(host))),
        host_(host) {}

  StatusIconHoverButton(const StatusIconHoverButton&) = delete;
  StatusIconHoverButton& operator=(const StatusIconHoverButton&) = delete;
  ~StatusIconHoverButton() override = default;

 protected:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->UndoDeviceScaleFactor();

    gfx::Rect bounds =
        host_->widget_->GetNativeWindow()->GetHost()->GetBoundsInPixels();
    const gfx::ImageSkia& image = host_->delegate_->GetImage();

    // If the image fits in the window, center it.  But if it won't fit,
    // downscale it preserving aspect ratio.
    float scale =
        std::min({1.0f, static_cast<float>(bounds.width()) / image.width(),
                  static_cast<float>(bounds.height()) / image.height()});
    float x_offset = (bounds.width() - scale * image.width()) / 2.0f;
    float y_offset = (bounds.height() - scale * image.height()) / 2.0f;

    gfx::Transform transform;
    transform.Translate(x_offset, y_offset);
    transform.Scale(scale, scale);
    canvas->Transform(transform);

    cc::PaintFlags flags;
    flags.setFilterQuality(cc::PaintFlags::FilterQuality::kHigh);
    canvas->DrawImageInt(image, 0, 0, image.width(), image.height(), 0, 0,
                         image.width(), image.height(), true, flags);
  }

 private:
  raw_ptr<StatusIconButtonLinux> host_;
};

BEGIN_METADATA(StatusIconButtonLinux, StatusIconHoverButton)
END_METADATA

StatusIconButtonLinux::StatusIconButtonLinux() = default;

StatusIconButtonLinux::~StatusIconButtonLinux() = default;

void StatusIconButtonLinux::SetImage(const gfx::ImageSkia& image) {
  if (button_) {
    button_->SchedulePaint();
  }
}

void StatusIconButtonLinux::SetIcon(const gfx::VectorIcon& icon) {
  SetImage(gfx::CreateVectorIcon(icon, SK_ColorWHITE));
}

void StatusIconButtonLinux::SetToolTip(const std::u16string& tool_tip) {
  if (button_) {
    button_->SetTooltipText(tool_tip);
  }
}

void StatusIconButtonLinux::UpdatePlatformContextMenu(ui::MenuModel* model) {
  // Nothing to do.
}

void StatusIconButtonLinux::OnSetDelegate() {
  // The delegate may have been cleared.
  if (!delegate_) {
    return;
  }

  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformRuntimeProperties()
           .supports_system_tray_windowing) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  widget_ = std::make_unique<StatusIconWidget>();

  const int width = std::max(1, delegate_->GetImage().width());
  const int height = std::max(1, delegate_->GetImage().height());

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.bounds =
      gfx::Rect(kInitialWindowPos, kInitialWindowPos, width, height);
  params.wm_role_name = ui::kStatusIconWmRoleName;
  params.wm_class_name = shell_integration_linux::GetProgramClassName();
  params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  // The status icon is a tiny window that doesn't update very often, so
  // creating a compositor would only be wasteful of resources.
  params.force_software_compositing = true;

  widget_->Init(std::move(params));

  // The window and host are non-null because the widget was just initialized.
  auto* host = widget_->GetNativeWindow()->GetHost();
  if (host->GetAcceleratedWidget() == gfx::kNullAcceleratedWidget) {
    delegate_->OnImplInitializationFailed();
    // |this| might be destroyed.
    return;
  }

  auto button = std::make_unique<StatusIconHoverButton>(this);
  button_ = button.get();

  button_->SetBorder(nullptr);
  button_->SetTooltipText(delegate_->GetToolTip());
  button_->set_context_menu_controller(this);

  widget_->SetContentsView(std::move(button));

  widget_->Show();
}

void StatusIconButtonLinux::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  ui::MenuModel* menu = delegate_->GetMenuModel();
  if (!menu) {
    return;
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu, views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU |
                views::MenuRunner::FIXED_ANCHOR);
  menu_runner_->RunMenuAt(widget_.get(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}
