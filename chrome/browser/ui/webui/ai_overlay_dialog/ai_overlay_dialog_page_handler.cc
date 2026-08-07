// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/jpeg_codec.h"
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
    page_->SetInputCaptionsVisible(controller->input_captions_visible());
    page_->SetOutputCaptionsVisible(controller->output_captions_visible());
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
        kActionShowAiOverlayDialog,
        BrowserActions::From(browser_)->root_action_item());
  }

  if (overlay_action_item_) {
    auto* controller = AiOverlayDialogController::From(browser_);
    const gfx::VectorIcon* base_icon =
        (controller && controller->IsOverlayShowing())
            ? &(features::IsRoundedIconsEnabled()
                    ? vector_icons::kPauseFilledIcon
                    : vector_icons::kPauseOldIcon)
            : &(features::IsRoundedIconsEnabled() ? vector_icons::kMicFilledIcon
                                                  : vector_icons::kMicOldIcon);

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

void AiOverlayDialogPageHandler::Close() {
  if (auto* controller = AiOverlayDialogController::From(browser_)) {
    // HideOverlay() turns off listening and closes the overlay WebUI dialog interface.
    // TODO(crbug.com/540858790): Rename HideOverlay() to CloseOverlay() for clarity.
    controller->HideOverlay();
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
    ai_overlay_dialog::mojom::PageContentNodePtr root_node) {
  VLOG(1) << "Update Current Page Context";
  VLOG(1) << "\tTitle: " << base::UTF16ToUTF8(title);

  page_->UpdateCurrentPageContext(base::UTF16ToUTF8(title),
                                  std::move(root_node));
}

void AiOverlayDialogPageHandler::OnInputCaptionsVisibleChanged(bool visible) {
  page_->SetInputCaptionsVisible(visible);
}

void AiOverlayDialogPageHandler::OnOutputCaptionsVisibleChanged(bool visible) {
  page_->SetOutputCaptionsVisible(visible);
}

void AiOverlayDialogPageHandler::OnUsePersonaChanged(bool use_persona) {
  page_->SetUsePersona(use_persona);
}

void AiOverlayDialogPageHandler::GetCursorPosition(
    GetCursorPositionCallback callback) {
  display::Screen* screen = display::Screen::Get();
  if (!screen) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  gfx::Point cursor_screen = screen->GetCursorScreenPoint();

  content::WebContents* web_contents =
      browser_ ? browser_->GetTabStripModel()->GetActiveWebContents()
               : nullptr;

  if (!web_contents) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  gfx::Rect tab_bounds = web_contents->GetContainerBounds();
  if (!tab_bounds.Contains(cursor_screen)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  gfx::Point cursor_local = cursor_screen - tab_bounds.OffsetFromOrigin();
  std::move(callback).Run(cursor_local);
}

void AiOverlayDialogPageHandler::CaptureRawViewportRegion(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    CaptureRawViewportRegionCallback callback) {
  content::WebContents* web_contents =
      browser_ ? browser_->GetTabStripModel()->GetActiveWebContents() : nullptr;

  if (!web_contents) {
    std::move(callback).Run(nullptr);
    return;
  }

  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  if (!view) {
    std::move(callback).Run(nullptr);
    return;
  }

  gfx::Rect tab_bounds = web_contents->GetContainerBounds();
  gfx::Rect crop_rect_logical(x, y, width, height);
  crop_rect_logical.Intersect(
      gfx::Rect(0, 0, tab_bounds.width(), tab_bounds.height()));

  float scale = view->GetDeviceScaleFactor();

  view->CopyFromSurface(
      crop_rect_logical, gfx::Size(), base::TimeDelta(),
      base::BindOnce(
          [](float scale, CaptureRawViewportRegionCallback cb,
             const content::CopyFromSurfaceResult& result) {
            if (!result.has_value() || result->bitmap.drawsNothing()) {
              std::move(cb).Run(nullptr);
              return;
            }

            const SkBitmap& bitmap = result->bitmap;
            std::optional<std::vector<uint8_t>> jpeg_bytes =
                gfx::JPEGCodec::Encode(bitmap.pixmap(), 85);
            if (!jpeg_bytes.has_value()) {
              std::move(cb).Run(nullptr);
              return;
            }

            std::string b64_data = base::Base64Encode(*jpeg_bytes);
            auto res = ai_overlay_dialog::mojom::RawViewportRegionResult::New();
            res->jpeg_data_b64 = b64_data;
            res->width = bitmap.width();
            res->height = bitmap.height();
            res->scale_factor = scale;
            std::move(cb).Run(std::move(res));
          },
          scale, std::move(callback)));
}

// TODO(crbug.com/542590634): Determine product and architecture requirements
// for long-term storage and persistence of remembered notes across restarts.
void AiOverlayDialogPageHandler::SetRememberedNote(
    ai_overlay_dialog::mojom::RememberedNotePtr note,
    SetRememberedNoteCallback callback) {
  if (!note || note->key.empty()) {
    std::move(callback).Run(false);
    return;
  }
  if (note->value.empty()) {
    remembered_notes_.erase(note->key);
  } else {
    remembered_notes_[note->key] = note->value;
  }
  std::move(callback).Run(true);
}

void AiOverlayDialogPageHandler::GetRememberedNotes(
    GetRememberedNotesCallback callback) {
  std::vector<ai_overlay_dialog::mojom::RememberedNotePtr> result;
  result.reserve(remembered_notes_.size());
  for (const auto& [key, value] : remembered_notes_) {
    auto note = ai_overlay_dialog::mojom::RememberedNote::New();
    note->key = key;
    note->value = value;
    result.push_back(std::move(note));
  }
  std::move(callback).Run(std::move(result));
}
}  // namespace ttc
