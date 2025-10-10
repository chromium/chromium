// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/new_tab_button.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

// Todo: set both non-hover and hover colours.
// When hover mode is on, start adjusting the colour towards the hover colour, but then
// start adjusting backward when hover is off.
// Also add presets for v60-era and v109-era themes.

// Diamond tabs and other esoteric shapes can be paid.

// static
const gfx::Size NewTabButton::kButtonSize{28, 28};

class NewTabButton::HighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator() = default;
  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const NewTabButton*>(view)->GetBorderPath(
        view->GetContentsBounds().origin(), false);
  }
};

typedef struct DataItem {
	int datakey; // index of point on the tab
	std::vector<float> values; // set of coordinate pairs
}DataItem;

bool GetNtbCustomStr(std::string& tabstr) {
    base::FilePath userdir;
    if(!base::PathService::Get(chrome::DIR_USER_DATA, &userdir))
        return false; // Things are seriously wrong if the user data directory cannot be located.
    const base::FilePath userpath = userdir.Append(FILE_PATH_LITERAL("scs"));
    std::string bufstr;
    base::ReadFileToString(userpath, &bufstr);

    if (bufstr.empty())
        return false;
    std::string::size_type sectionstart = bufstr.find("ntb");
    std::string::size_type sectionend = bufstr.find("endntb");

   if (sectionstart == std::string::npos || sectionend == std::string::npos)
      return false;

    tabstr.append(bufstr.substr(sectionstart, sectionend - sectionstart).c_str());
    return true;
}

bool UserDefinedNtbClipRect(gfx::Rect& visibleRect, std::string& sectionContent) {
    std::string::size_type pos = 0;
    while ((pos = sectionContent.find("{", pos)) != std::string::npos) {
        std::string::size_type endPos = sectionContent.find("}", pos);
        if (endPos == std::string::npos) {
            break;
        }

        std::string dataItemStr = sectionContent.substr(pos + 1, endPos - pos - 1);
        std::string::size_type equalPos = dataItemStr.find('=');

        if (equalPos != std::string::npos) {
            DataItem dataItem;
         std::string key = dataItemStr.substr(0, equalPos);
         if(!std::all_of(key.begin(), key.end(), [](char c) {return c >= '0' && c <= '9';}))
             break;
            dataItem.datakey = std::stoi(key);
            std::string valuesStr = dataItemStr.substr(equalPos + 1);

            // Parse the values
            std::istringstream valuesStream(valuesStr);
            std::string value;
            while (std::getline(valuesStream, value, ',')) {
            if(!std::all_of(value.begin(), value.end(), [](char c) {return (c >= '0' && c <= '9') || c == '.';}))
               break; // No exceptions, so we must verify that there are only integers in the value before continuing.
                       // If not, the config file is considered to be "compromised" and needs to be replaced or fixed.
                     // We will not tolerate any unexpected values in the data.
            if(value.size() > 6)
               break; // Any "unnecessarily large" numbers will also be blocked.
            float val = std::stof(value);

                dataItem.values.push_back(val);
            }

         if (dataItem.values.size() != 6)
            break; // Fail if there is an incorrect quantity of values in the data item.

         if (dataItem.datakey == 1) {
            // bit 0 (0x1) is used to indicate that the line segment represented by the item should be visible;
            // all other points will be clipped out of the visible rectangle.
            visibleRect.SetRect(dataItem.values.at(0) < visibleRect.x() ? dataItem.values.at(0) : visibleRect.x()
                            , dataItem.values.at(1) > visibleRect.y() ? dataItem.values.at(1) : visibleRect.y()
                            , dataItem.values.at(4) > visibleRect.right() ? dataItem.values.at(4) : visibleRect.right()
                            , dataItem.values.at(5) < visibleRect.bottom() ? dataItem.values.at(5) : visibleRect.bottom());
         }

        }

        pos = endPos + 1;
    }

   return true;
}

bool UserDefinedNtbShape(SkPath& path, std::string& sectionContent) {
    std::string::size_type pos = 0;
    while ((pos = sectionContent.find("{", pos)) != std::string::npos) {
        std::string::size_type endPos = sectionContent.find("}", pos);
        if (endPos == std::string::npos) {
            break;
        }

        std::string dataItemStr = sectionContent.substr(pos + 1, endPos - pos - 1);
        std::string::size_type equalPos = dataItemStr.find('=');

        if (equalPos != std::string::npos) {
            DataItem dataItem;
         std::string key = dataItemStr.substr(0, equalPos);
         if(!std::all_of(key.begin(), key.end(), [](char c) {return c >= '0' && c <= '9';}))
             break;
            dataItem.datakey = std::stoi(key);
            std::string valuesStr = dataItemStr.substr(equalPos + 1);

            // Parse the values
            std::istringstream valuesStream(valuesStr);
            std::string value;
            while (std::getline(valuesStream, value, ',')) {
            if(!std::all_of(value.begin(), value.end(), [](char c) {return (c >= '0' && c <= '9') || c == '.';}))
               break; // No exceptions, so we must verify that there are only integers in the value before continuing.
                       // If not, the config file is considered to be "compromised" and needs to be replaced or fixed.
                     // We will not tolerate any unexpected values in the data.
            if(value.size() > 6)
               break; // Any "unnecessarily large" numbers will also be blocked.
            float val = std::stof(value);

                dataItem.values.push_back(val);
            }

         if (dataItem.values.size() != 6)
            break; // Fail if there is an incorrect quantity of values in the data item.

         path.cubicTo(dataItem.values.at(0), dataItem.values.at(1), dataItem.values.at(2),
                      dataItem.values.at(3), dataItem.values.at(4), dataItem.values.at(5));
        }

        pos = endPos + 1;
    }

   return true;
}

NewTabButton::NewTabButton(TabStrip* tab_strip, PressedCallback callback)
    : views::ImageButton(std::move(callback)), tab_strip_(tab_strip) {
  SetAnimateOnStateChange(true);

  // If there is an image for the NewTabButton it is set by the theme. Theme
  // images should not be flipped for RTL.
  SetFlipCanvasOnPaintForRTLUI(false);

  foreground_frame_active_color_id_ = kColorNewTabButtonForegroundFrameActive;
  foreground_frame_inactive_color_id_ =
      kColorNewTabButtonForegroundFrameInactive;
  background_frame_active_color_id_ = kColorNewTabButtonBackgroundFrameActive;
  background_frame_inactive_color_id_ =
      kColorNewTabButtonBackgroundFrameInactive;

  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") != "v60" &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") != "rectangular") {
      views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
      views::InkDrop::Get(this)->SetHighlightOpacity(0.16f);
      views::InkDrop::Get(this)->SetVisibleOpacity(0.14f);

      SetInstallFocusRingOnFocus(true);
      views::HighlightPathGenerator::Install(
          this, std::make_unique<NewTabButton::HighlightPathGenerator>());
  }
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  SetProperty(views::kElementIdentifierKey, kNewTabButtonElementId);
}

NewTabButton::~NewTabButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

void NewTabButton::FrameColorsChanged() {
  const auto* const color_provider = GetColorProvider();
  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);
  views::InkDrop::Get(this)->SetBaseColor(
      color_provider->GetColor(GetWidget()->ShouldPaintAsActive()
                                   ? kColorNewTabButtonInkDropFrameActive
                                   : kColorNewTabButtonInkDropFrameInactive));
  SchedulePaint();
}

void NewTabButton::AnimateToStateForTesting(views::InkDropState state) {
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(state);
}

void NewTabButton::AddLayerToRegion(ui::Layer* new_layer,
                                    views::LayerRegion region) {
  ink_drop_container_->AddLayerToRegion(new_layer, region);
}

void NewTabButton::RemoveLayerFromRegions(ui::Layer* old_layer) {
  ink_drop_container_->RemoveLayerFromRegions(old_layer);
}

SkColor NewTabButton::GetForegroundColor() const {
  return GetColorProvider()->GetColor(
      GetWidget()->ShouldPaintAsActive() ? foreground_frame_active_color_id_
                                         : foreground_frame_inactive_color_id_);
}

int NewTabButton::GetCornerRadius() const {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, GetContentsBounds().size());
}

SkPath NewTabButton::GetBorderPath(const gfx::Point& origin,
                                   bool extend_to_top) const {
  float radius = GetCornerRadius();

  SkPath path;
  std::string ntbstr;
  bool is_custom_str_available = false; 
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("enable-advanced-customization")) {
      is_custom_str_available = GetNtbCustomStr(ntbstr);
  }
  UserDefinedNtbShape(path, ntbstr);

  if (is_custom_str_available && !path.isEmpty()) {
      return path;
  }

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "v60") {
    SkPoint pts [] = {SkPoint(4.0, 8.0), SkPoint(24.0, 8.0), SkPoint(28.0, 20.0), SkPoint(8.0, 20.0)};
    path.moveTo(origin.x() * 1.10f, SkScalar(8.0));
    path.addPoly(pts, 4, true);
    return path;
  }

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "rectangular") {
    SkPoint pts [] = {SkPoint(8.0, 8.0), SkPoint(32.0, 8.0), SkPoint(32.0, 20.0), SkPoint(8.0, 20.0)};
    path.moveTo(origin.x() * 1.10f, SkScalar(8.0));
    path.addPoly(pts, 4, true);
    return path;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch("compact-tab-ui")) {
    radius -= 2;
  }

  if (extend_to_top) {
    path.moveTo(origin.x(), 0);
    const float diameter = radius * 2;
    path.rLineTo(diameter, 0);
    path.rLineTo(0, origin.y() + radius);
    path.rArcTo(radius, radius, 0, SkPath::kSmall_ArcSize, SkPathDirection::kCW,
                -diameter, 0);
    path.close();
  } else {
    path.addCircle(origin.x() + radius, origin.y() + radius, radius);
  }
  return path;
}

void NewTabButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ImageButton::OnBoundsChanged(previous_bounds);
  ink_drop_container_->SetBoundsRect(GetLocalBounds());
}

void NewTabButton::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &NewTabButton::FrameColorsChanged, base::Unretained(this)));
}

void NewTabButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void NewTabButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  FrameColorsChanged();
}

#if BUILDFLAG(IS_WIN)
void NewTabButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event.IsOnlyRightMouseButton()) {
    views::ImageButton::OnMouseReleased(event);
    return;
  }

  // TODO(pkasting): If we handled right-clicks on the frame, and we made sure
  // this event was not handled, it seems like things would Just Work.
  gfx::Point point = event.location();
  views::View::ConvertPointToScreen(this, &point);
  point = display::win::GetScreenWin()->DIPToScreenPoint(point);
  auto weak_this = weak_factory_.GetWeakPtr();
  views::ShowSystemMenuAtScreenPixelLocation(views::HWNDForView(this), point);
  if (!weak_this) {
    return;
  }
  SetState(views::Button::STATE_NORMAL);
}
#endif

void NewTabButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  views::ImageButton::OnGestureEvent(event);
  event->SetHandled();
}

void NewTabButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::ACTION_TRIGGERED);
}

void NewTabButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintFill(canvas);
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") != "v60" &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") != "rectangular") {
     PaintIcon(canvas);
  }
}

gfx::Size NewTabButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = kButtonSize;
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

bool NewTabButton::GetHitTestMask(SkPath* mask) const {
  DCHECK(mask);

  gfx::Point origin = GetContentsBounds().origin();
  if (base::i18n::IsRTL()) {
    origin.set_x(GetInsets().right());
  }
  SkPath border =
      GetBorderPath(origin, tab_strip_->controller()->IsFrameCondensed());
  mask->addPath(border);
  return true;
}

void NewTabButton::PaintFill(gfx::Canvas* canvas) const {
  gfx::ScopedCanvas scoped_canvas(canvas);

  const std::optional<int> bg_id =
      tab_strip_->GetCustomBackgroundId(BrowserFrameActiveState::kUseCurrent);
  if (bg_id.has_value()) {
    // The shape and location of the background texture is defined by a clip
    // path. This needs to be translated to center it in the view.
    auto path = GetBorderPath(gfx::Point(), false);
    auto offset = GetContentsBounds().OffsetFromOrigin();
    path.offset(offset.x(), offset.y());
    canvas->ClipPath(path, /*do_anti_alias=*/true);
    if (base::CommandLine::ForCurrentProcess()->HasSwitch("enable-advanced-customization")) {
      std::string tab_str;
      gfx::Rect visible_rect;
	  bool is_custom_str_available = GetNtbCustomStr(tab_str);
      if (is_custom_str_available) {
        UserDefinedNtbClipRect(visible_rect, tab_str);
        canvas->ClipRect(visible_rect, SkClipOp::kIntersect);
      }
    }
    // But the background image itself must not be translated in order to align
    // with the same background image painted across different views.
    TopContainerBackground::PaintThemeAlignedImage(
        canvas, this,
        BrowserView::GetBrowserViewForBrowser(tab_strip_->GetBrowser()),
        GetThemeProvider()->GetImageSkiaNamed(bg_id.value()));
  } else {
    // In the non themed-image case, we simply draw a solid color button. Note
    // that cc::PaintFlags defaults to fill.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    canvas->Translate(GetContentsBounds().OffsetFromOrigin());

    if (GetState() == STATE_HOVERED) {
        flags.setColor(SkColorSetARGB(0x80, 90, 90, 90));
    } else {
        if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "v60" ||
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "rectangular") {
            flags.setColor(GetColorProvider()->GetColor(
                GetWidget()->ShouldPaintAsActive()
                   ? background_frame_inactive_color_id_
                   : background_frame_active_color_id_));
       } else {
            flags.setColor(GetColorProvider()->GetColor(
                GetWidget()->ShouldPaintAsActive()
                   ? background_frame_active_color_id_
                   : background_frame_inactive_color_id_));
       }
    }
    if (base::CommandLine::ForCurrentProcess()->HasSwitch("transparent-tabs")) {
        flags.setAlphaf(0.7f);
    }
    canvas->DrawPath(GetBorderPath(gfx::Point(), false), flags);
    if (GetLayoutConstant(TAB_HARD_BORDER)) {
        flags.setColor(SkColorSetRGB(0, 0, 0));    
        flags.setAlphaf(0.7f);
        flags.setStyle(cc::PaintFlags::kStroke_Style);
        canvas->DrawPath(GetBorderPath(gfx::Point(), false),
                         flags);
    }
  }
}

void NewTabButton::PaintIcon(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->Translate(GetContentsBounds().OffsetFromOrigin());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetForegroundColor());
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  constexpr int kStrokeWidth = 2;
  flags.setStrokeWidth(kStrokeWidth);

  int radius = ui::TouchUiController::Get()->touch_ui() ? 7 : 6;
  const int offset = GetCornerRadius() - radius;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("compact-tab-ui")) {
    radius -= 2;
  }
  // The cap will be added outside the end of the stroke; inset to compensate.
  constexpr int kCapRadius = kStrokeWidth / 2;
  const int start = offset + kCapRadius;
  const int end = offset + (radius * 2) - kCapRadius;
  const int center = offset + radius;

  // Horizontal stroke.
  canvas->DrawLine(gfx::PointF(start, center), gfx::PointF(end, center), flags);

  // Vertical stroke.
  canvas->DrawLine(gfx::PointF(center, start), gfx::PointF(center, end), flags);
}

BEGIN_METADATA(NewTabButton)
END_METADATA