// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/menus/simple_menu_model.h"
#include "url/url_util.h"

namespace {

class AnimatedIconSource : public gfx::CanvasImageSource {
 public:
  static constexpr int kIconSize = ui::SimpleMenuModel::kDefaultIconSize;  // 16
  static constexpr int kCanvasSize = 22;

  AnimatedIconSource(const gfx::VectorIcon* icon, float energy, SkColor color)
      : CanvasImageSource(gfx::Size(kCanvasSize, kCanvasSize)),
        icon_(icon),
        energy_(energy),
        color_(color) {}

  void Draw(gfx::Canvas* canvas) override {
    // 1. Draw the vector icon in the center of our expanded canvas
    int offset = (kCanvasSize - kIconSize) / 2;
    canvas->Save();
    canvas->Translate(gfx::Vector2d(offset, offset));
    gfx::PaintVectorIcon(canvas, *icon_, kIconSize, color_);
    canvas->Restore();

    // 2. Draw the animated circle if energy is present
    if (energy_ > 0.01f) {
      // Scale from 1.0 to 1.4 of the *icon's* original bounds
      float radius_fraction = 1.0f + (0.4f * energy_);
      float base_radius = kIconSize / 2.0f;
      float radius = base_radius * radius_fraction;

      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(1.5f);
      flags.setColor(SK_ColorWHITE);

      // Draw relative to the expanded canvas center
      gfx::PointF center(kCanvasSize / 2.0f, kCanvasSize / 2.0f);
      canvas->DrawCircle(center, radius, flags);
    }
  }

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  float energy_;
  SkColor color_;
};

}  // namespace

namespace ttc {

AiOverlayDialogPageHandler::AiOverlayDialogPageHandler(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
    mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote,
    BrowserWindowInterface* browser)
    : receiver_(this, std::move(receiver)),
      page_(std::move(remote)),
      browser_(browser) {
  if (auto* controller = AiOverlayDialogController::From(browser_)) {
    controller->AddObserver(this);
    page_->SetCaptionsVisible(controller->captions_visible());
    page_->SetUsePersona(controller->use_persona());
  }
}

AiOverlayDialogPageHandler::~AiOverlayDialogPageHandler() {
  if (auto* controller = AiOverlayDialogController::From(browser_)) {
    controller->RemoveObserver(this);
  }
}

void AiOverlayDialogPageHandler::GetMockAudioData(
    GetMockAudioDataCallback callback) {
  std::string path_string = features::kAiOverlayDialogMockJsonPath.Get();
  std::replace(path_string.begin(), path_string.end(), '+', '/');
  if (path_string.empty()) {
    VLOG(1) << "MockAudioData path not specified";
    std::move(callback).Run(std::nullopt);
    return;
  }

  VLOG(1) << "Using MockAudioData from: " << path_string;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const std::string& path_string) -> std::optional<std::string> {
            std::string data;
            if (!base::ReadFileToString(
                    base::FilePath::FromUTF8Unsafe(path_string), &data)) {
              return std::nullopt;
            }

            VLOG(1) << "\tMockAudioData head: " << data.substr(0, 100);
            return data;
          },
          path_string),
      std::move(callback));
}

void AiOverlayDialogPageHandler::UpdateAudioEnergy(float energy) {
  if (!overlay_action_item_) {
    overlay_action_item_ = actions::ActionManager::Get().FindAction(
        kActionShowAiOverlayDialog, browser_->GetActions()->root_action_item());
  }

  if (overlay_action_item_) {
    auto* controller = AiOverlayDialogController::From(browser_);
    const gfx::VectorIcon* base_icon =
        (controller && controller->IsOverlayShowing())
            ? &vector_icons::kPauseIcon
            : &vector_icons::kMicIcon;

    overlay_action_item_->SetImage(ui::ImageModel::FromImageGenerator(
        base::BindRepeating(
            [](const gfx::VectorIcon* icon, float current_energy,
               const ui::ColorProvider* color_provider) -> gfx::ImageSkia {
              SkColor icon_color = color_provider->GetColor(ui::kColorIcon);
              return gfx::CanvasImageSource::MakeImageSkia<AnimatedIconSource>(
                  icon, current_energy, icon_color);
            },
            base_icon, energy),
        gfx::Size(AnimatedIconSource::kCanvasSize,
                  AnimatedIconSource::kCanvasSize)));
  }
}

void AiOverlayDialogPageHandler::DidChangePage(
    const GURL& url,
    const std::optional<std::u16string>& title,
    const std::optional<std::string>& content) {
  VLOG(1) << "Did Change Page";
  VLOG(1) << "\tURL: " << url.spec();
  if (title.has_value()) {
    VLOG(1) << "\tTitle: " << base::UTF16ToUTF8(title.value());
  }
  if (content.has_value()) {
    VLOG(1) << "\tContent: " << content.value().substr(0, 200) << "...";
  }

  page_->DidChangePage(
      url.spec(),
      title.has_value() ? std::make_optional(base::UTF16ToUTF8(title.value()))
                        : std::nullopt,
      content);
}

void AiOverlayDialogPageHandler::UpdateCurrentPageContext(
    const std::u16string& title,
    const std::string& content) {
  VLOG(1) << "Update Current Page Context";
  VLOG(1) << "\tTitle: " << base::UTF16ToUTF8(title);
  VLOG(1) << "\tContent: " << content.substr(0, 200) << "...";

  page_->UpdateCurrentPageContext(base::UTF16ToUTF8(title), content);
}

void AiOverlayDialogPageHandler::OnCaptionsVisibleChanged(bool visible) {
  page_->SetCaptionsVisible(visible);
}

void AiOverlayDialogPageHandler::OnUsePersonaChanged(bool use_persona) {
  page_->SetUsePersona(use_persona);
}

}  // namespace ttc
