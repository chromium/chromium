// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/buildflags.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

// The CaptionBubbleLabel needs to be focusable in order for NVDA to enable
// document navigation. It is suspected that other screen readers on Windows and
// Linux will need this behavior, too. VoiceOver and ChromeVox do not need the
// label to be focusable.
#if BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY() && !BUILDFLAG(IS_MAC)
#define NEED_FOCUS_FOR_ACCESSIBILITY
#endif

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
#include "base/scoped_observation.h"
#include "ui/accessibility/platform/ax_platform.h"
#endif

namespace {

// Formatting constants
static constexpr int kLineHeightDip = 24;
static constexpr int kLiveTranslateLabelLineHeightDip = 18;
static constexpr int kLiveTranslateImageWidthDip = 16;
static constexpr int kLanguageButtonImageLabelSpacing = 4;
static constexpr int kNumLinesCollapsed = 2;
static constexpr int kNumLinesExpanded = 8;
static constexpr int kCornerRadiusDip = 4;
static constexpr int kSidePaddingDip = 18;
static constexpr int kButtonDip = 16;
static constexpr int kButtonCircleHighlightPaddingDip = 2;
static constexpr int kMaxWidthDip = 536;
// Margin of the bubble with respect to the context window.
static constexpr int kMinAnchorMarginDip = 20;
static constexpr char kPrimaryFont[] = "Roboto";
static constexpr char kSecondaryFont[] = "Arial";
static constexpr char kTertiaryFont[] = "sans-serif";
static constexpr int kFontSizePx = 16;
static constexpr int kLiveTranslateLabelFontSizePx = 11;
static constexpr double kDefaultRatioInParentX = 0.5;
static constexpr double kDefaultRatioInParentY = 1;
static constexpr int kErrorImageSizeDip = 20;
static constexpr int kErrorMessageBetweenChildSpacingDip = 16;
static constexpr int kNoActivityIntervalSeconds = 5;

constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(250);

std::unique_ptr<views::ImageButton> BuildImageButton(
    views::Button::PressedCallback callback,
    const int tooltip_text_id) {
  auto button = views::CreateVectorImageButton(std::move(callback));
  button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
  button->SizeToPreferredSize();
  views::InstallCircleHighlightPathGenerator(
      button.get(), gfx::Insets(kButtonCircleHighlightPaddingDip));
  return button;
}

// ui::CaptionStyle styles are CSS strings that can sometimes have !important.
// This method removes " !important" if it exists at the end of a CSS string.
std::string MaybeRemoveCSSImportant(std::string css_string) {
  RE2::Replace(&css_string, "\\s+!important", "");
  return css_string;
}

// Parses a CSS color string that is in the form rgba and has a non-zero alpha
// value into an SkColor and sets it to be the value of the passed-in SkColor
// address. Returns whether or not the operation was a success.
//
// `css_string` is the CSS color string passed in. It is in the form
//     rgba(#,#,#,#). r, g, and b are integers between 0 and 255, inclusive.
//     a is a double between 0.0 and 1.0. There may be whitespace in between the
//     commas.
// `sk_color` is the address of an SKColor. If the operation is a success, the
//     function will set sk_color's value to be the parsed SkColor. If the
//     operation is not a success, sk_color's value will not change.
//
// As of spring 2021, all OS's use rgba to define the caption style colors.
// However, the ui::CaptionStyle spec also allows for the use of any valid CSS
// color spec. This function will need to be revisited should ui::CaptionStyle
// colors use non-rgba to define their colors.
bool ParseNonTransparentRGBACSSColorString(
    std::string css_string,
    SkColor* sk_color,
    const ui::ColorProvider* color_provider) {
  std::string rgba = MaybeRemoveCSSImportant(css_string);
  if (rgba.empty())
    return false;
  uint16_t r, g, b;
  double a;
  bool match = RE2::FullMatch(
      rgba, "rgba\\((\\d+),\\s*(\\d+),\\s*(\\d+),\\s*(\\d+\\.?\\d*)\\)", &r, &g,
      &b, &a);
  // If the opacity is set to 0 (fully transparent), we ignore the user's
  // preferred style and use our default color.
  if (!match || a == 0)
    return false;
  uint16_t a_int = base::ClampRound<uint16_t>(a * 255);
#if BUILDFLAG(IS_MAC)
  // On Mac, any opacity lower than 90% leaves rendering artifacts which make
  // it appear like there is a layer of faint text beneath the actual text.
  // TODO(crbug.com/40177817): Fix the rendering issue and then remove this
  // workaround.
  a_int = std::max(static_cast<uint16_t>(SkColorGetA(color_provider->GetColor(
                       ui::kColorLiveCaptionBubbleBackgroundDefault))),
                   a_int);
#endif
  *sk_color = SkColorSetARGB(a_int, r, g, b);
  return match;
}

// Helper class for observing mouse and key events from native window.
class CaptionBubbleEventObserver : public ui::EventObserver {
 public:
  explicit CaptionBubbleEventObserver(captions::CaptionBubble* caption_bubble,
                                      views::Widget* widget)
      : caption_bubble_(caption_bubble) {
    CHECK(widget);
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, widget->GetNativeWindow(),
        {ui::EventType::kMouseMoved, ui::EventType::kMouseExited,
         ui::EventType::kKeyPressed, ui::EventType::kKeyReleased});
  }

  CaptionBubbleEventObserver(const CaptionBubbleEventObserver&) = delete;
  CaptionBubbleEventObserver& operator=(const CaptionBubbleEventObserver&) =
      delete;
  ~CaptionBubbleEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
    if (event.IsKeyEvent()) {
      caption_bubble_->UpdateControlsVisibility(true);
      return;
    }

    // We check if the mouse is in bounds rather than strictly
    // checking mouse enter/exit events because of two reasons: 1. We get
    // mouse exit/enter events when the mouse moves between client and
    // non-client areas on Linux and Windows; 2. We get a mouse exit event when
    // a context menu is brought up, which might cause the caption bubble to be
    // stuck in the "in" state when some other window is on top of the caption
    // bubble.
    caption_bubble_->OnMouseEnteredOrExitedWindow(IsMouseInBounds());
  }

 private:
  bool IsMouseInBounds() {
    gfx::Point point = event_monitor_->GetLastMouseLocation();
    views::View::ConvertPointFromScreen(caption_bubble_, &point);

    return caption_bubble_->GetLocalBounds().Contains(point);
  }

  raw_ptr<captions::CaptionBubble> caption_bubble_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

}  // namespace

namespace captions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsCaptionBubbleKey, false)
#endif

#if BUILDFLAG(IS_WIN)
class MediaFoundationRendererErrorMessageView : public views::StyledLabel {
  METADATA_HEADER(MediaFoundationRendererErrorMessageView, views::StyledLabel)

 public:
  explicit MediaFoundationRendererErrorMessageView(
      CaptionBubble* caption_bubble)
      : caption_bubble_(caption_bubble) {}

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    switch (action_data.action) {
      case ax::mojom::Action::kDoDefault:
        caption_bubble_->OnContentSettingsLinkClicked();
        return true;
      default:
        break;
    }
    return views::StyledLabel::HandleAccessibleAction(action_data);
  }

 private:
  const raw_ptr<CaptionBubble> caption_bubble_;  // Not owned.
};

BEGIN_METADATA(MediaFoundationRendererErrorMessageView)
END_METADATA

#endif

// CaptionBubble implementation of BubbleFrameView. This class takes care
// of making the caption draggable.
class CaptionBubbleFrameView : public views::BubbleFrameView {
  METADATA_HEADER(CaptionBubbleFrameView, views::BubbleFrameView)

 public:
  explicit CaptionBubbleFrameView(
      std::vector<raw_ptr<views::View, VectorExperimental>> buttons,
      ResetInactivityTimerCallback reset_inactivity_timer_cb)
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()),
        buttons_(buttons),
        reset_inactivity_timer_cb_(std::move(reset_inactivity_timer_cb)) {
    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::DIALOG_SHADOW);
    border->SetCornerRadius(kCornerRadiusDip);
    views::BubbleFrameView::SetBubbleBorder(std::move(border));
  }

  ~CaptionBubbleFrameView() override = default;
  CaptionBubbleFrameView(const CaptionBubbleFrameView&) = delete;
  CaptionBubbleFrameView& operator=(const CaptionBubbleFrameView&) = delete;

  void OnMouseExited(const ui::MouseEvent& event) override {
    reset_inactivity_timer_cb_.Run();
  }

  // TODO(crbug.com/40119836): This does not work on Linux because the bubble is
  // not a top-level view, so it doesn't receive events. See crbug.com/1074054
  // for more about why it doesn't work.
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // |point| is in coordinates relative to CaptionBubbleFrameView, i.e.
    // (0,0) is the upper left corner of this view. Convert it to screen
    // coordinates to see whether one of the buttons contains this point.
    // If it is, return HTCLIENT, so that the click is sent through to be
    // handled by CaptionBubble::BubblePressed().
    gfx::Point point_in_screen =
        GetBoundsInScreen().origin() + gfx::Vector2d(point.x(), point.y());
    for (views::View* button : buttons_) {
      if (button->GetBoundsInScreen().Contains(point_in_screen))
        return HTCLIENT;
    }

    // Ensure it's within the BubbleFrameView. This takes into account the
    // rounded corners and drop shadow of the BubbleBorder.
    int hit = views::BubbleFrameView::NonClientHitTest(point);

    // After BubbleFrameView::NonClientHitTest processes the bubble-specific
    // hits such as the rounded corners, it checks hits to the bubble's client
    // view. Any hits to ClientFrameView::NonClientHitTest return HTCLIENT or
    // HTNOWHERE. Override these to return HTCAPTION in order to make the
    // entire widget draggable.
    return (hit == HTCLIENT || hit == HTNOWHERE) ? HTCAPTION : hit;
  }

 private:
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons_;
  ResetInactivityTimerCallback reset_inactivity_timer_cb_;
};

BEGIN_METADATA(CaptionBubbleFrameView)
END_METADATA

class CaptionBubbleLabelAXModeObserver;

// CaptionBubble implementation of Label. This class takes care of setting up
// the accessible virtual views of the label in order to support braille
// accessibility. The CaptionBubbleLabel is a readonly document with a paragraph
// inside. Inside the paragraph are staticText nodes, one for each visual line
// in the rendered text of the label. These staticText nodes are shown on a
// braille display so that a braille user can read the caption text line by
// line.
class CaptionBubbleLabel : public views::Label {
  METADATA_HEADER(CaptionBubbleLabel, views::Label)

 public:
  CaptionBubbleLabel() {
    GetViewAccessibility().SetRole(ax::mojom::Role::kDocument);
    GetViewAccessibility().SetName(
        std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    GetViewAccessibility().SetReadOnly(true);
#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
    ax_mode_observer_ =
        std::make_unique<CaptionBubbleLabelAXModeObserver>(this);
    SetFocusBehaviorForAccessibility();
#endif
  }
  ~CaptionBubbleLabel() override = default;
  CaptionBubbleLabel(const CaptionBubbleLabel&) = delete;
  CaptionBubbleLabel& operator=(const CaptionBubbleLabel&) = delete;

  void SetMinimumHeight(int height) {
    if (minimum_height_ == height) {
      return;
    }

    minimum_height_ = height;
    PreferredSizeChanged();
  }

  void SetText(const std::u16string& text) override {
    views::Label::SetText(text);

    auto& ax_lines = GetViewAccessibility().virtual_children();
    if (text.empty() && !ax_lines.empty()) {
      GetViewAccessibility().RemoveAllVirtualChildViews();
      NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
      return;
    }

    const size_t num_lines = GetRequiredLines();
    size_t start = 0;
    for (size_t i = 0; i < num_lines - 1; ++i) {
      size_t end = GetTextIndexOfLine(i + 1);
      std::u16string substring = text.substr(start, end - start);
      UpdateAXLine(substring, i, gfx::Range(start, end));
      start = end;
    }
    std::u16string substring = text.substr(start, text.size() - start);
    if (!substring.empty()) {
      UpdateAXLine(substring, num_lines - 1, gfx::Range(start, text.size()));
    }

    // Remove all ax_lines that don't have a corresponding line.
    size_t num_ax_lines = ax_lines.size();
    for (size_t i = num_lines; i < num_ax_lines; ++i) {
      GetViewAccessibility().RemoveVirtualChildView(ax_lines.back().get());
      NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
    }
  }

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
  // The CaptionBubbleLabel needs to be focusable in order for NVDA to enable
  // document navigation. Making the CaptionBubbleLabel focusable means it gets
  // a tabstop, so it should only be focusable for screen reader users.
  void SetFocusBehaviorForAccessibility() {
    SetFocusBehavior(ui::AXPlatform::GetInstance().GetMode().has_mode(
                         ui::AXMode::kScreenReader)
                         ? FocusBehavior::ALWAYS
                         : FocusBehavior::NEVER);
  }
#endif

 private:
  void UpdateAXLine(const std::u16string& line_text,
                    const size_t line_index,
                    const gfx::Range& text_range) {
    auto& ax_lines = GetViewAccessibility().virtual_children();

    // Add a new virtual child for a new line of text.
    DCHECK(line_index <= ax_lines.size());
    if (line_index == ax_lines.size()) {
      auto ax_line = std::make_unique<views::AXVirtualView>();
      ax_line->GetCustomData().role = ax::mojom::Role::kStaticText;
      GetViewAccessibility().AddVirtualChildView(std::move(ax_line));
      NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
    }

    // Set the virtual child's name as line text.
    ui::AXNodeData& ax_node_data = ax_lines[line_index]->GetCustomData();
    if (base::UTF8ToUTF16(ax_node_data.GetStringAttribute(
            ax::mojom::StringAttribute::kName)) != line_text) {
      ax_node_data.SetNameChecked(line_text);
      std::vector<gfx::Rect> bounds = GetSubstringBounds(text_range);
      ax_node_data.relative_bounds.bounds = gfx::RectF(bounds[0]);
      ax_lines[line_index]->NotifyAccessibilityEvent(
          ax::mojom::Event::kTextChanged);
    }
  }

  // Label:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size preferred_size =
        views::Label::CalculatePreferredSize(available_size);
    preferred_size.set_height(
        std::max(minimum_height_, preferred_size.height()));
    return preferred_size;
  }

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
  std::unique_ptr<CaptionBubbleLabelAXModeObserver> ax_mode_observer_;
#endif
  int minimum_height_ = 0;
};

BEGIN_METADATA(CaptionBubbleLabel)
END_METADATA

class LanguageLabelButton : public views::LabelButton {
  METADATA_HEADER(LanguageLabelButton, views::LabelButton)

 public:
  LanguageLabelButton(views::LabelButton::PressedCallback callback)
      : views::LabelButton(std::move(callback)) {
    label()->SetMultiLine(false);
    label()->SetBackgroundColor(SK_ColorTRANSPARENT);
    label()->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    label()->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);

    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnHover(true);
    views::InkDrop::Get(this)->SetLayerRegion(views::LayerRegion::kAbove);
    SetHasInkDropActionOnClick(true);

    SetImageLabelSpacing(kLanguageButtonImageLabelSpacing);

    SetPaintToLayer();
  }

  LanguageLabelButton(const LanguageLabelButton&) = delete;
  LanguageLabelButton& operator=(const LanguageLabelButton&) = delete;
  ~LanguageLabelButton() override = default;

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  void SetBaseColor() {
    const auto* const color_provider = GetColorProvider();
    views::InkDrop::Get(this)->SetBaseColor(
        color_provider->GetColor(ui::kColorLiveCaptionBubbleForegroundDefault));
  }

  void SetTextScaleFactor(double text_scale_factor) {
    SetFocusRingCornerRadius(text_scale_factor * kLineHeightDip / 2);
  }

  void SetTranslateIconVisible(bool visible) {
    image_container_view()->SetVisible(visible);

    if (visible) {
      image_container_view()->SetPreferredSize(
          gfx::Size(kButtonDip, kButtonDip));
    } else {
      image_container_view()->SetPreferredSize(gfx::Size(0, 0));
    }
  }

  views::Label* GetLabel() { return label(); }
};

BEGIN_METADATA(LanguageLabelButton)
END_METADATA

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
// A helper class to the CaptionBubbleLabel which observes AXMode changes and
// updates the CaptionBubbleLabel focus behavior in response.
// TODO(crbug.com/40756389): Implement a ui::AXModeObserver::OnAXModeRemoved
// method which observes the removal of AXModes. Without that, the caption
// bubble label will remain focusable once accessibility is enabled, even if
// accessibility is later disabled.
class CaptionBubbleLabelAXModeObserver : public ui::AXModeObserver {
 public:
  explicit CaptionBubbleLabelAXModeObserver(CaptionBubbleLabel* owner)
      : owner_(owner) {
    ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
  }

  ~CaptionBubbleLabelAXModeObserver() override = default;

  CaptionBubbleLabelAXModeObserver(const CaptionBubbleLabelAXModeObserver&) =
      delete;
  CaptionBubbleLabelAXModeObserver& operator=(
      const CaptionBubbleLabelAXModeObserver&) = delete;

  void OnAXModeAdded(ui::AXMode mode) override {
    owner_->SetFocusBehaviorForAccessibility();
  }

 private:
  raw_ptr<CaptionBubbleLabel> owner_;
  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      ax_mode_observation_{this};
};
#endif

CaptionBubble::CaptionBubble(PrefService* profile_prefs,
                             const std::string& application_locale,
                             base::OnceClosure destroyed_callback)
    : views::BubbleDialogDelegateView(nullptr,
                                      views::BubbleBorder::TOP_LEFT,
                                      views::BubbleBorder::DIALOG_SHADOW,
                                      true),
      profile_prefs_(profile_prefs),
      destroyed_callback_(std::move(destroyed_callback)),
      application_locale_(application_locale),
      is_expanded_(
          profile_prefs_->GetBoolean(prefs::kLiveCaptionBubbleExpanded)),
      is_pinned_(profile_prefs_->GetBoolean(prefs::kLiveCaptionBubblePinned)),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      controls_animation_(this) {
  // Bubbles that use transparent colors should not paint their ClientViews to a
  // layer as doing so could result in visual artifacts.
  SetPaintClientToLayer(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  // While not shown, the title is still used to identify the window in the
  // window switcher.
  SetShowTitle(false);
  SetTitle(IDS_LIVE_CAPTION_BUBBLE_TITLE);
  set_has_parent(false);

  controls_animation_.SetSlideDuration(kAnimationDuration);
  controls_animation_.SetTweenType(gfx::Tween::LINEAR);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);
  pref_change_registrar_->Add(
      prefs::kLiveTranslateEnabled,
      base::BindRepeating(&CaptionBubble::OnLiveTranslateEnabledChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveCaptionLanguageCode,
      base::BindRepeating(&CaptionBubble::OnLiveCaptionLanguageChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveTranslateTargetLanguageCode,
      base::BindRepeating(&CaptionBubble::OnLiveTranslateTargetLanguageChanged,
                          base::Unretained(this)));
  inactivity_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, base::Seconds(kNoActivityIntervalSeconds),
      base::BindRepeating(&CaptionBubble::OnInactivityTimeout,
                          base::Unretained(this)),
      tick_clock_);
  inactivity_timer_->Stop();
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
}

CaptionBubble::~CaptionBubble() {
  if (model_)
    model_->RemoveObserver();
}

gfx::Rect CaptionBubble::GetBubbleBounds() {
  // Bubble bounds are what the computed bubble bounds would be, taking into
  // account the current bubble size.
  gfx::Rect bubble_bounds = views::BubbleDialogDelegateView::GetBubbleBounds();
  // Widget bounds are where the bubble currently is in space.
  gfx::Rect widget_bounds = GetWidget()->GetWindowBoundsInScreen();
  // Use the widget x and y to keep the bubble oriented at its current location,
  // and use the bubble width and height to set the correct bubble size.
  return gfx::Rect(widget_bounds.x(), widget_bounds.y(), bubble_bounds.width(),
                   bubble_bounds.height());
}

void CaptionBubble::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
  UseCompactMargins();
  set_close_on_deactivate(false);
  SetCanActivate(true);

  views::View* header_container = new views::View();
  header_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  views::View* right_header_container = new views::View();
  views::View* left_header_container = new views::View();

  views::View* content_container = new views::View();
  content_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(gfx::Insets::VH(0, kSidePaddingDip))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width*/ true));

  auto label = std::make_unique<CaptionBubbleLabel>();
  label->SetMultiLine(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);
  label->SetTooltipText(std::u16string());
  // Render text truncates the end of text that is greater than 10000 chars.
  // While it is unlikely that the text will exceed 10000 chars, it is not
  // impossible, if the speech service sends a very long transcription_result.
  // In order to guarantee that the caption bubble displays the last lines, and
  // in order to ensure that caption_bubble_->GetTextIndexOfLine() is correct,
  // set the truncate_length to 0 to ensure that it never truncates.
  label->SetTruncateLength(0);

  auto title = std::make_unique<views::Label>();
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));
  title->GetViewAccessibility().SetIsIgnored(true);

  // Define an error message that will be displayed in the caption bubble if a
  // generic error is encountered.
  auto generic_error_text = std::make_unique<views::Label>();
  generic_error_text->SetBackgroundColor(SK_ColorTRANSPARENT);
  generic_error_text->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  generic_error_text->SetText(
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_ERROR));
  auto generic_error_message = std::make_unique<views::View>();
  generic_error_message
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kErrorMessageBetweenChildSpacingDip))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  generic_error_message->SetVisible(false);
  auto generic_error_icon = std::make_unique<views::ImageView>();

#if BUILDFLAG(IS_WIN)
  // Define an error message that will be displayed in the caption bubble if the
  // renderer is using hardware-based decryption.
  auto media_foundation_renderer_error_message =
      std::make_unique<views::View>();
  media_foundation_renderer_error_message
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kErrorMessageBetweenChildSpacingDip))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  media_foundation_renderer_error_message->SetVisible(false);
  auto media_foundation_renderer_error_icon =
      std::make_unique<views::ImageView>();
  auto media_foundation_renderer_error_text =
      std::make_unique<MediaFoundationRendererErrorMessageView>(this);
  media_foundation_renderer_error_text->SetAutoColorReadabilityEnabled(false);
  media_foundation_renderer_error_text->SetSubpixelRenderingEnabled(false);
  media_foundation_renderer_error_text->SetFocusBehavior(FocusBehavior::ALWAYS);
  media_foundation_renderer_error_text->SetTextContext(
      views::style::CONTEXT_DIALOG_BODY_TEXT);

  // Make the whole text view behave as a link for accessibility.
  media_foundation_renderer_error_text->GetViewAccessibility().SetRole(
      ax::mojom::Role::kLink);

  const std::u16string link =
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_CONTENT_SETTINGS);

  media_foundation_renderer_error_text->SetText(l10n_util::GetStringFUTF16(
      IDS_LIVE_CAPTION_BUBBLE_MEDIA_FOUNDATION_RENDERER_ERROR, link));

  auto media_foundation_renderer_error_checkbox =
      std::make_unique<views::Checkbox>(
          l10n_util::GetStringUTF16(
              IDS_LIVE_CAPTION_BUBBLE_MEDIA_FOUNDATION_RENDERER_ERROR_CHECKBOX),
          base::BindRepeating(
              &CaptionBubble::MediaFoundationErrorCheckboxPressed,
              base::Unretained(this)));
#endif

  base::RepeatingClosure expand_or_collapse_callback = base::BindRepeating(
      &CaptionBubble::ExpandOrCollapseButtonPressed, base::Unretained(this));
  auto expand_button = BuildImageButton(expand_or_collapse_callback,
                                        IDS_LIVE_CAPTION_BUBBLE_EXPAND);
  expand_button->SetVisible(!is_expanded_);

  auto collapse_button = BuildImageButton(
      std::move(expand_or_collapse_callback), IDS_LIVE_CAPTION_BUBBLE_COLLAPSE);
  collapse_button->SetVisible(is_expanded_);

  auto back_to_tab_button = BuildImageButton(
      base::BindRepeating(&CaptionBubble::BackToTabButtonPressed,
                          base::Unretained(this)),
      IDS_LIVE_CAPTION_BUBBLE_BACK_TO_TAB);
  back_to_tab_button->SetVisible(false);

  auto close_button =
      BuildImageButton(base::BindRepeating(&CaptionBubble::CloseButtonPressed,
                                           base::Unretained(this)),
                       IDS_LIVE_CAPTION_BUBBLE_CLOSE);

  base::RepeatingClosure pin_or_unpin_callback = base::BindRepeating(
      &CaptionBubble::PinOrUnpinButtonPressed, base::Unretained(this));
  auto pin_button =
      BuildImageButton(pin_or_unpin_callback, IDS_LIVE_CAPTION_BUBBLE_PIN);
  pin_button->SetVisible(!is_pinned_);
  pin_button_ = right_header_container->AddChildView(std::move(pin_button));

  auto unpin_button = BuildImageButton(std::move(pin_or_unpin_callback),
                                       IDS_LIVE_CAPTION_BUBBLE_UNPIN);
  unpin_button->SetVisible(is_pinned_);
  unpin_button_ = right_header_container->AddChildView(std::move(unpin_button));

  back_to_tab_button_ =
      right_header_container->AddChildView(std::move(back_to_tab_button));
  close_button_ = right_header_container->AddChildView(std::move(close_button));

  title_ = content_container->AddChildView(std::move(title));
  label_ = content_container->AddChildView(std::move(label));

  auto download_progress_label = std::make_unique<views::Label>();
  download_progress_label->SetBackgroundColor(SK_ColorTRANSPARENT);
  download_progress_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  download_progress_label->SetVerticalAlignment(
      gfx::VerticalAlignment::ALIGN_MIDDLE);
  download_progress_label->SetVisible(false);
  download_progress_label_ =
      content_container->AddChildView(std::move(download_progress_label));

  generic_error_icon_ =
      generic_error_message->AddChildView(std::move(generic_error_icon));
  generic_error_text_ =
      generic_error_message->AddChildView(std::move(generic_error_text));
  generic_error_message_ =
      content_container->AddChildView(std::move(generic_error_message));

#if BUILDFLAG(IS_WIN)
  media_foundation_renderer_error_icon_ =
      media_foundation_renderer_error_message->AddChildView(
          std::move(media_foundation_renderer_error_icon));

  auto inner_box_layout = std::make_unique<views::BoxLayoutView>();
  inner_box_layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  inner_box_layout->SetBetweenChildSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  media_foundation_renderer_error_text_ = inner_box_layout->AddChildView(
      std::move(media_foundation_renderer_error_text));
  media_foundation_renderer_error_checkbox_ = inner_box_layout->AddChildView(
      std::move(media_foundation_renderer_error_checkbox));
  media_foundation_renderer_error_message->AddChildView(
      std::move(inner_box_layout));
  media_foundation_renderer_error_message_ = content_container->AddChildView(
      std::move(media_foundation_renderer_error_message));
#endif

  expand_button_ = content_container->AddChildView(std::move(expand_button));
  collapse_button_ =
      content_container->AddChildView(std::move(collapse_button));

  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    auto language_label = std::make_unique<LanguageLabelButton>(
        base::BindRepeating(&CaptionBubble::CaptionSettingsButtonPressed,
                            base::Unretained(this)));
    language_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    language_label->GetViewAccessibility().SetIsIgnored(true);

    source_language_code_ =
        profile_prefs_->GetString(prefs::kLiveCaptionLanguageCode);
    source_language_text_ = speech::GetLanguageDisplayName(
        source_language_code_, application_locale_);
    target_language_code_ =
        profile_prefs_->GetString(prefs::kLiveTranslateTargetLanguageCode);
    target_language_text_ = speech::GetLanguageDisplayName(
        target_language_code_, application_locale_);
    language_label_ =
        left_header_container->AddChildView(std::move(language_label));
    OnLanguageChanged();
  }

  std::unique_ptr<views::BoxLayout> right_header_container_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal);
  right_header_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  right_header_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  right_header_container->SetLayoutManager(
      std::move(right_header_container_layout));
  std::unique_ptr<views::BoxLayout> left_header_container_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(
              0, close_button_->GetBorder()->GetInsets().width() / 2, 0, 0));
  left_header_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  left_header_container->SetLayoutManager(
      std::move(left_header_container_layout));

  left_header_container_ =
      header_container->AddChildView(std::move(left_header_container));
  header_container->AddChildView(std::move(right_header_container));
  header_container_ = AddChildView(std::move(header_container));
  AddChildView(std::move(content_container));

  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    std::vector<raw_ptr<views::View, VectorExperimental>> buttons =
        GetButtons();
    for (views::View* button : buttons) {
      button->SetPaintToLayer();
      button->layer()->SetFillsBoundsOpaquely(false);
      button->layer()->SetOpacity(0);
    }

    language_label_->layer()->SetFillsBoundsOpaquely(false);
    download_progress_label_->SetPaintToLayer();
    download_progress_label_->layer()->SetFillsBoundsOpaquely(false);
    download_progress_label_->layer()->SetOpacity(0);
  }

  UpdateContentSize();
  UpdateAccessibleName();
  title_text_changed_callback_ =
      title_->AddTextChangedCallback(base::BindRepeating(
          &CaptionBubble::OnTitleTextChanged, weak_ptr_factory_.GetWeakPtr()));
}

void CaptionBubble::OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                             views::Widget* widget) const {
  params->type = views::Widget::InitParams::TYPE_WINDOW;
  params->z_order = ui::ZOrderLevel::kFloatingWindow;
  params->visible_on_all_workspaces = true;
  params->name = "LiveCaptionWindow";
#if BUILDFLAG(IS_CHROMEOS_ASH)
  params->init_properties_container.SetProperty(kIsCaptionBubbleKey, true);
#endif
}

bool CaptionBubble::ShouldShowCloseButton() const {
  // We draw our own close button so that we can capture the button presses and
  // so we can customize its appearance.
  return false;
}

std::unique_ptr<views::NonClientFrameView>
CaptionBubble::CreateNonClientFrameView(views::Widget* widget) {
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons = GetButtons();
  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    caption_bubble_event_observer_ =
        std::make_unique<CaptionBubbleEventObserver>(this, widget);
  }

  auto frame = std::make_unique<CaptionBubbleFrameView>(
      buttons, base::BindRepeating(&CaptionBubble::ResetInactivityTimer,
                                   base::Unretained(this)));
  frame_ = frame.get();
  return frame;
}

void CaptionBubble::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget, GetWidget());

  // If the widget is visible and unfocused, probably due to a mouse drag, reset
  // the inactivity timer.
  if (GetWidget()->IsVisible() && !HasFocus())
    ResetInactivityTimer();
}

void CaptionBubble::OnWidgetActivationChanged(views::Widget* widget,
                                              bool active) {
  DCHECK_EQ(widget, GetWidget());

  if (!active && mouse_inside_window_) {
    active = true;
  }

  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    UpdateControlsVisibility(active);
  }

  ResetInactivityTimer();
}

void CaptionBubble::OnLiveTranslateEnabledChanged() {
  OnLanguageChanged();
  SetTextColor();
  Redraw();
}

void CaptionBubble::OnLiveCaptionLanguageChanged() {
  auto_detected_source_language_ = false;
  source_language_code_ =
      profile_prefs_->GetString(prefs::kLiveCaptionLanguageCode);
  source_language_text_ = speech::GetLanguageDisplayName(source_language_code_,
                                                         application_locale_);

  OnLanguageChanged();
  SetTextColor();
  Redraw();
}

void CaptionBubble::OnLiveTranslateTargetLanguageChanged() {
  target_language_code_ =
      profile_prefs_->GetString(prefs::kLiveTranslateTargetLanguageCode);
  target_language_text_ = speech::GetLanguageDisplayName(target_language_code_,
                                                         application_locale_);

  OnLanguageChanged();
  SetTextColor();
  Redraw();
}

std::u16string CaptionBubble::GetAccessibleWindowTitle() const {
  return title_->GetText();
}

void CaptionBubble::OnThemeChanged() {
  if (ThemeColorsChanged()) {
    SetCaptionBubbleStyle();
  }

  // Call this after SetCaptionButtonStyle(), not before, since
  // SetCaptionButtonStyle() calls set_color(), which OnThemeChanged() will
  // trigger a read of.
  views::BubbleDialogDelegateView::OnThemeChanged();
}

void CaptionBubble::BackToTabButtonPressed() {
  DCHECK(model_);
  DCHECK(model_->GetContext()->IsActivatable());
  model_->GetContext()->Activate();
}

void CaptionBubble::CloseButtonPressed() {
  LogSessionEvent(SessionEvent::kCloseButtonClicked);
  if (model_)
    model_->CloseButtonPressed();

#if BUILDFLAG(IS_CHROMEOS)
  profile_prefs_->SetBoolean(prefs::kLiveCaptionEnabled, false);
#endif
}

void CaptionBubble::ExpandOrCollapseButtonPressed() {
  is_expanded_ = !is_expanded_;
  profile_prefs_->SetBoolean(prefs::kLiveCaptionBubbleExpanded, is_expanded_);
  base::UmaHistogramBoolean("Accessibility.LiveCaption.ExpandBubble",
                            is_expanded_);

  SwapButtons(collapse_button_, expand_button_, is_expanded_);

  // The change of expanded state may cause the title to change visibility, and
  // it surely causes the content height to change, so redraw the bubble.
  Redraw();
}

void CaptionBubble::PinOrUnpinButtonPressed() {
  is_pinned_ = !is_pinned_;
  profile_prefs_->SetBoolean(prefs::kLiveCaptionBubblePinned, is_pinned_);
  base::UmaHistogramBoolean("Accessibility.LiveCaption.PinBubble", is_pinned_);

  SwapButtons(unpin_button_, pin_button_, is_pinned_);
}

void CaptionBubble::SwapButtons(views::Button* first_button,
                                views::Button* second_button,
                                bool show_first_button) {
  if (!show_first_button)
    std::swap(first_button, second_button);

  second_button->SetVisible(false);
  first_button->SetVisible(true);
  ResetInactivityTimer();

  if (!first_button->HasFocus())
    first_button->RequestFocus();
}

void CaptionBubble::CaptionSettingsButtonPressed() {
  model_->GetContext()->GetOpenCaptionSettingsCallback().Run();
}

void CaptionBubble::SetModel(CaptionBubbleModel* model) {
  if (model_)
    model_->RemoveObserver();
  model_ = model;
  if (model_) {
    model_->SetObserver(this);
    back_to_tab_button_->SetVisible(model_->GetContext()->IsActivatable());
    UpdateLanguageLabelText();
  } else {
    UpdateBubbleVisibility();
  }
}

void CaptionBubble::AnimationProgressed(const gfx::Animation* animation) {
  if (!media::IsLiveTranslateEnabled() &&
      !base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    return;
  }
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons = GetButtons();
  for (views::View* button : buttons) {
    button->layer()->SetOpacity(animation->GetCurrentValue());
  }
  language_label_->layer()->SetOpacity(animation->GetCurrentValue());
  download_progress_label_->layer()->SetOpacity(animation->GetCurrentValue());
}

void CaptionBubble::OnTextChanged() {
  DCHECK(model_);
  std::string text = model_->GetFullText();
  label_->SetText(base::UTF8ToUTF16(text));
  UpdateBubbleAndTitleVisibility();

  if (GetWidget()->IsVisible())
    ResetInactivityTimer();
}

void CaptionBubble::OnDownloadProgressTextChanged() {
  if (!media::IsLiveTranslateEnabled()) {
    return;
  }

  DCHECK(model_);
  download_progress_label_->SetText(model_->GetDownloadProgressText());
  download_progress_label_->SetVisible(true);

  // Do not display captions while language packs are downloading.
  label_->SetVisible(false);

  UpdateBubbleAndTitleVisibility();

  if (GetWidget()->IsVisible()) {
    ResetInactivityTimer();
    UpdateControlsVisibility(true);
  }
}

void CaptionBubble::OnLanguagePackInstalled() {
  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    download_progress_label_->SetVisible(false);
    label_->SetVisible(true);
  }
}

void CaptionBubble::OnAutoDetectedLanguageChanged() {
  if (!media::IsLiveTranslateEnabled() &&
      !base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    return;
  }

  source_language_code_ = model_->GetAutoDetectedLanguageCode();
  source_language_text_ = speech::GetLanguageDisplayName(source_language_code_,
                                                         application_locale_);

  auto_detected_source_language_ =
      l10n_util::GetLanguage(
          profile_prefs_->GetString(prefs::kLiveCaptionLanguageCode)) !=
      l10n_util::GetLanguage(source_language_code_);

  OnLanguageChanged();
  SetTextColor();
  Redraw();
}

bool CaptionBubble::ThemeColorsChanged() {
  const auto* const color_provider = GetColorProvider();
  SkColor text_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleForegroundDefault);
  SkColor icon_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleButtonIcon);
  SkColor icon_disabled_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleButtonIconDisabled);
  SkColor link_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleLink);
  SkColor checkbox_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleCheckbox);
  SkColor background_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleBackgroundDefault);

  bool theme_colors_changed =
      text_color != text_color_ || icon_color != icon_color_ ||
      icon_disabled_color != icon_disabled_color_ ||
      link_color != link_color_ || checkbox_color != checkbox_color_ ||
      background_color != background_color_;

  text_color_ = text_color;
  icon_color_ = icon_color;
  icon_disabled_color_ = icon_disabled_color;
  link_color_ = link_color;
  checkbox_color_ = checkbox_color;
  background_color_ = background_color;

  return theme_colors_changed;
}

void CaptionBubble::OnErrorChanged(
    CaptionBubbleErrorType error_type,
    OnErrorClickedCallback callback,
    OnDoNotShowAgainClickedCallback error_silenced_callback) {
  DCHECK(model_);
  error_clicked_callback_ = std::move(callback);
  error_silenced_callback_ = std::move(error_silenced_callback);
  bool has_error = model_->HasError();
  label_->SetVisible(!has_error);
  expand_button_->SetVisible(!has_error && !is_expanded_);
  collapse_button_->SetVisible(!has_error && is_expanded_);

#if BUILDFLAG(IS_WIN)
  if (error_type ==
      CaptionBubbleErrorType::kMediaFoundationRendererUnsupported) {
    media_foundation_renderer_error_message_->SetVisible(has_error);
    generic_error_message_->SetVisible(false);
  } else {
    generic_error_message_->SetVisible(has_error);
    media_foundation_renderer_error_message_->SetVisible(false);
  }
#else
  generic_error_message_->SetVisible(has_error);
#endif

  Redraw();
}

#if BUILDFLAG(IS_WIN)
void CaptionBubble::OnContentSettingsLinkClicked() {
  if (error_clicked_callback_) {
    error_clicked_callback_.Run();
  }
}
#endif

void CaptionBubble::UpdateControlsVisibility(bool show_controls) {
  if (show_controls) {
    controls_animation_.Show();
  } else {
    controls_animation_.Hide();
  }
}

void CaptionBubble::OnMouseEnteredOrExitedWindow(bool entered) {
  mouse_inside_window_ = entered;
  UpdateControlsVisibility(mouse_inside_window_);
}

void CaptionBubble::UpdateBubbleAndTitleVisibility() {
  // Show the title if there is room for it and no error.
  title_->SetVisible(model_ && !model_->HasError() &&
                     GetNumLinesInLabel() <
                         static_cast<size_t>(GetNumLinesVisible()));
  UpdateBubbleVisibility();
}

void CaptionBubble::UpdateBubbleVisibility() {
  DCHECK(GetWidget());
  // If there is no model set, do not show the bubble.
  if (!model_) {
    Hide();
    return;
  }

  // Hide the widget if the model is closed or the bubble has no activity.
  // Activity is defined as transcription received from the speech service or
  // user interacting with the bubble through focus, pressing buttons, or
  // dragging.
  if (model_->IsClosed() || !HasActivity()) {
    Hide();
    return;
  }

  // Show the widget if it has text or an error or download progress to display.
  if (!model_->GetFullText().empty() || model_->HasError() ||
      (media::IsLiveTranslateEnabled() &&
       download_progress_label_->GetVisible())) {
    ShowInactive();
    return;
  }

  // No text and no error. Hide it.
  Hide();
}

void CaptionBubble::UpdateCaptionStyle(
    std::optional<ui::CaptionStyle> caption_style) {
  caption_style_ = caption_style;
  SetCaptionBubbleStyle();
  Redraw();
}

size_t CaptionBubble::GetTextIndexOfLineInLabel(size_t line) const {
  return label_->GetTextIndexOfLine(line);
}

size_t CaptionBubble::GetNumLinesInLabel() const {
  return label_->GetRequiredLines();
}

int CaptionBubble::GetNumLinesVisible() {
  return is_expanded_ ? kNumLinesExpanded : kNumLinesCollapsed;
}

double CaptionBubble::GetTextScaleFactor() {
  double textScaleFactor = 1;
  if (caption_style_) {
    std::string text_size = MaybeRemoveCSSImportant(caption_style_->text_size);
    if (!text_size.empty()) {
      // ui::CaptionStyle states that text_size is percentage as a CSS string.
      bool match =
          RE2::FullMatch(text_size, "(\\d+\\.?\\d*)%", &textScaleFactor);
      textScaleFactor = match ? textScaleFactor / 100 : 1;
    }
  }
  return textScaleFactor;
}
const gfx::FontList CaptionBubble::GetFontList(int font_size) {
  std::vector<std::string> font_names;
  if (caption_style_) {
    std::string font_family =
        MaybeRemoveCSSImportant(caption_style_->font_family);
    if (!font_family.empty())
      font_names.push_back(font_family);
  }
  font_names.push_back(kPrimaryFont);
  font_names.push_back(kSecondaryFont);
  font_names.push_back(kTertiaryFont);

  const gfx::FontList font_list = gfx::FontList(
      font_names, gfx::Font::FontStyle::NORMAL,
      font_size * GetTextScaleFactor(), gfx::Font::Weight::NORMAL);
  return font_list;
}

void CaptionBubble::SetTextSizeAndFontFamily() {
  double textScaleFactor = GetTextScaleFactor();
  const gfx::FontList font_list = GetFontList(kFontSizePx);
  label_->SetFontList(font_list);
  title_->SetFontList(font_list.DeriveWithStyle(gfx::Font::FontStyle::ITALIC));
  generic_error_text_->SetFontList(font_list);

  label_->SetLineHeight(kLineHeightDip * textScaleFactor);
  label_->SetMaximumWidth(kMaxWidthDip * textScaleFactor - kSidePaddingDip * 2);
  title_->SetLineHeight(kLineHeightDip * textScaleFactor);
  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    download_progress_label_->SetLineHeight(kLiveTranslateLabelLineHeightDip *
                                            textScaleFactor);
    download_progress_label_->SetFontList(
        GetFontList(kLiveTranslateLabelFontSizePx));
    language_label_->GetLabel()->SetLineHeight(
        kLiveTranslateLabelLineHeightDip * textScaleFactor);
    language_label_->GetLabel()->SetFontList(
        GetFontList(kLiveTranslateLabelFontSizePx));
    language_label_->SetTextScaleFactor(textScaleFactor);
  }
  generic_error_text_->SetLineHeight(kLineHeightDip * textScaleFactor);
  generic_error_icon_->SetImageSize(
      gfx::Size(kErrorImageSizeDip * textScaleFactor,
                kErrorImageSizeDip * textScaleFactor));

#if BUILDFLAG(IS_WIN)
  media_foundation_renderer_error_icon_->SetImageSize(
      gfx::Size(kErrorImageSizeDip, kErrorImageSizeDip));
  media_foundation_renderer_error_text_->SizeToFit(
      kMaxWidthDip * textScaleFactor - kSidePaddingDip * 2);
#endif
}

void CaptionBubble::SetTextColor() {
  const auto* const color_provider = GetColorProvider();
  SkColor primary_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleForegroundDefault);
  SkColor icon_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleButtonIcon);
  SkColor icon_disabled_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleButtonIconDisabled);

  // Update Live Translate label style with the default colors before parsing
  // the CSS color string.
  download_progress_label_->SetEnabledColor(primary_color);

  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    language_label_->SetBaseColor();
    language_label_->SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kTranslateIcon, icon_color,
                                       kLiveTranslateImageWidthDip));
    language_label_->SetImageModel(
        views::Button::ButtonState::STATE_HOVERED,
        ui::ImageModel::FromVectorIcon(vector_icons::kTranslateIcon,
                                       primary_color,
                                       kLiveTranslateImageWidthDip));
    language_label_->SetTextColor(views::Button::ButtonState::STATE_NORMAL,
                                  icon_color);
    language_label_->SetTextColor(views::Button::ButtonState::STATE_HOVERED,
                                  primary_color);
  }

  if (caption_style_) {
    ParseNonTransparentRGBACSSColorString(caption_style_->text_color,
                                          &primary_color, color_provider);
  }

  label_->SetEnabledColor(primary_color);
  title_->SetEnabledColor(primary_color);
  generic_error_text_->SetEnabledColor(primary_color);

  generic_error_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kErrorOutlineIcon, primary_color));

#if BUILDFLAG(IS_WIN)

  const std::u16string link =
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_CONTENT_SETTINGS);
  size_t offset;
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_LIVE_CAPTION_BUBBLE_MEDIA_FOUNDATION_RENDERER_ERROR,
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_CONTENT_SETTINGS),
      &offset);

  media_foundation_renderer_error_text_->ClearStyleRanges();
  views::StyledLabel::RangeStyleInfo error_message_style;
  error_message_style.override_color = primary_color;
  media_foundation_renderer_error_text_->AddStyleRange(gfx::Range(0, offset),
                                                       error_message_style);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&CaptionBubble::OnContentSettingsLinkClicked,
                              base::Unretained(this)));
  link_style.override_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleLink);
  media_foundation_renderer_error_text_->AddStyleRange(
      gfx::Range(offset, offset + link.length()), link_style);

  media_foundation_renderer_error_text_->AddStyleRange(
      gfx::Range(offset + link.length(), text.length()), error_message_style);
  media_foundation_renderer_error_icon_->SetImage(
      gfx::CreateVectorIcon(vector_icons::kErrorOutlineIcon, primary_color));
  media_foundation_renderer_error_checkbox_->SetEnabledTextColors(
      primary_color);
  media_foundation_renderer_error_checkbox_->SetTextSubpixelRenderingEnabled(
      false);
  media_foundation_renderer_error_checkbox_->SetCheckedIconImageColor(
      color_provider->GetColor(ui::kColorLiveCaptionBubbleCheckbox));
#endif
  views::SetImageFromVectorIconWithColor(
      back_to_tab_button_, vector_icons::kBackToTabIcon, kButtonDip, icon_color,
      icon_disabled_color);
  views::SetImageFromVectorIconWithColor(
      close_button_, vector_icons::kCloseRoundedIcon, kButtonDip, icon_color,
      icon_disabled_color);
  views::SetImageFromVectorIconWithColor(
      expand_button_, vector_icons::kCaretDownIcon, kButtonDip, icon_color,
      icon_disabled_color);
  views::SetImageFromVectorIconWithColor(collapse_button_,
                                         vector_icons::kCaretUpIcon, kButtonDip,
                                         icon_color, icon_disabled_color);
  views::SetImageFromVectorIconWithColor(pin_button_, views::kPinIcon,
                                         kButtonDip, icon_color,
                                         icon_disabled_color);
  views::SetImageFromVectorIconWithColor(unpin_button_, views::kUnpinIcon,
                                         kButtonDip, icon_color,
                                         icon_disabled_color);
}

void CaptionBubble::SetBackgroundColor() {
  const auto* const color_provider = GetColorProvider();
  SkColor background_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleBackgroundDefault);
  if (caption_style_ &&
      !ParseNonTransparentRGBACSSColorString(
          caption_style_->window_color, &background_color, color_provider)) {
    ParseNonTransparentRGBACSSColorString(caption_style_->background_color,
                                          &background_color, color_provider);
  }
  set_color(background_color);
}

void CaptionBubble::OnLanguageChanged() {
  UpdateLanguageLabelText();

  // Update label text direction.
  std::string display_language =
      profile_prefs_->GetBoolean(prefs::kLiveTranslateEnabled)
          ? target_language_code_
          : source_language_code_;
  label_->SetHorizontalAlignment(
      base::i18n::GetTextDirectionForLocale(display_language.c_str()) ==
              base::i18n::TextDirection::RIGHT_TO_LEFT
          ? gfx::HorizontalAlignment::ALIGN_RIGHT
          : gfx::HorizontalAlignment::ALIGN_LEFT);
}

void CaptionBubble::UpdateLanguageLabelText() {
  const bool live_translate_enabled = media::IsLiveTranslateEnabled();
  // We update the language text and set it whenever live translate OR
  // multilingual live captions are enabled. We early out when both are
  // disabled.
  if (!live_translate_enabled &&
      !base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    return;
  }

  if (live_translate_enabled &&
      profile_prefs_->GetBoolean(prefs::kLiveTranslateEnabled) &&
      l10n_util::GetLanguage(source_language_code_) !=
          l10n_util::GetLanguage(target_language_code_)) {
    language_label_->SetText(l10n_util::GetStringFUTF16(
        auto_detected_source_language_
            ? IDS_LIVE_CAPTION_TRANSLATED_CAPTION_LANGUAGE_AUTODETECTED
            : IDS_LIVE_CAPTION_TRANSLATED_CAPTION_LANGUAGE,
        source_language_text_, target_language_text_));
    language_label_->SetTranslateIconVisible(true);
  } else {
    CHECK(language_label_ != nullptr);
    if (auto_detected_source_language_) {
      language_label_->SetText(l10n_util::GetStringFUTF16(
          IDS_LIVE_CAPTION_CAPTION_LANGUAGE_AUTODETECTED,
          source_language_text_));
    } else {
      language_label_->SetText(source_language_text_);
    }
    language_label_->SetTranslateIconVisible(false);
  }
}

void CaptionBubble::RepositionInContextRect(CaptionBubbleModel::Id model_id,
                                            const gfx::Rect& context_rect) {
  // We shouldn't reposition ourselves into the context rect of a model that is
  // no longer active.
  if (model_ == nullptr || model_->unique_id() != model_id) {
    return;
  }

  gfx::Rect inset_rect = context_rect;
  inset_rect.Inset(gfx::Insets(kMinAnchorMarginDip));
  gfx::Rect bubble_bounds = GetBubbleBounds();

  // The placement is based on the ratio between the center of the widget and
  // the center of the inset_rect.
  int target_x = inset_rect.x() + inset_rect.width() * kDefaultRatioInParentX -
                 bubble_bounds.width() / 2.0;
  int target_y = inset_rect.y() + inset_rect.height() * kDefaultRatioInParentY -
                 bubble_bounds.height() / 2.0;
  gfx::Rect target_bounds = gfx::Rect(target_x, target_y, bubble_bounds.width(),
                                      bubble_bounds.height());
  if (!inset_rect.Contains(target_bounds)) {
    target_bounds.AdjustToFit(inset_rect);
  }

  GetWidget()->SetBounds(target_bounds);
}

void CaptionBubble::UpdateContentSize() {
  double text_scale_factor = GetTextScaleFactor();
  int width = kMaxWidthDip * text_scale_factor;
  int content_height =
      kLineHeightDip * GetNumLinesVisible() * text_scale_factor;

  // The title takes up 1 line.
  int label_height = title_->GetVisible()
                         ? content_height - kLineHeightDip * text_scale_factor
                         : content_height;
  label_->SetMinimumHeight(label_height);
  auto button_size = close_button_->GetPreferredSize({});
  auto left_header_width = width - 3 * button_size.width();
  left_header_container_->SetPreferredSize(
      gfx::Size(left_header_width, button_size.height()));

  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    language_label_->SetPreferredSize(
        language_label_->CalculatePreferredSize({}));
  }

#if BUILDFLAG(IS_WIN)
  // The Media Foundation renderer error message should not scale with the
  // user's caption style preference.
  if (HasMediaFoundationError()) {
    width = kMaxWidthDip;
    content_height = media_foundation_renderer_error_message_
                         ->GetPreferredSize(
                             views::SizeBounds(width - kSidePaddingDip * 2, {}))
                         .height();
  }
#endif

  // The header height is the same as the close button height. The footer height
  // is the same as the expand button height.
  SetPreferredSize(gfx::Size(
      width, content_height + close_button_->GetPreferredSize({}).height() +
                 expand_button_->GetPreferredSize({}).height()));
}

void CaptionBubble::Redraw() {
  UpdateBubbleAndTitleVisibility();
  UpdateContentSize();
}

void CaptionBubble::ShowInactive() {
  DCHECK(model_);
  if (GetWidget()->IsVisible())
    return;

  GetWidget()->ShowInactive();
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_LIVE_CAPTION_BUBBLE_APPEAR_SCREENREADER_ANNOUNCEMENT));
  LogSessionEvent(SessionEvent::kStreamStarted);

  // If the caption bubble has already been shown, do not reposition it.
  if (has_been_shown_)
    return;
  has_been_shown_ = true;

  // The first time that the caption bubble is shown, reposition it to the
  // bottom center of the context widget for the currently set model.
  model_->GetContext()->GetBounds(
      base::BindOnce(&CaptionBubble::RepositionInContextRect,
                     weak_ptr_factory_.GetWeakPtr(), model_->unique_id()));
}

void CaptionBubble::Hide() {
  if (!GetWidget()->IsVisible())
    return;
  GetWidget()->Hide();
  LogSessionEvent(SessionEvent::kStreamEnded);
}

void CaptionBubble::OnInactivityTimeout() {
  if (HasMediaFoundationError() || IsMouseHovered() || is_pinned_ ||
      GetWidget()->IsActive())
    return;

  // Clear the partial and final text in the caption bubble model and the label.
  // Does not affect the speech service. The speech service will emit a final
  // result after ~10-15 seconds of no audio which the caption bubble will
  // receive but will not display. If the speech service is in the middle of a
  // recognition phrase, and the caption bubble regains activity (such as if the
  // audio stream restarts), the speech service will emit partial results that
  // contain text cleared by the UI.
  if (model_)
    model_->ClearText();

  Hide();
}

void CaptionBubble::ResetInactivityTimer() {
  inactivity_timer_->Reset();
}

void CaptionBubble::MediaFoundationErrorCheckboxPressed() {
#if BUILDFLAG(IS_WIN)
  error_silenced_callback_.Run(
      CaptionBubbleErrorType::kMediaFoundationRendererUnsupported,
      media_foundation_renderer_error_checkbox_->GetChecked());
#endif
}

bool CaptionBubble::HasMediaFoundationError() {
  return (model_ && model_->HasError() &&
          model_->ErrorType() ==
              CaptionBubbleErrorType::kMediaFoundationRendererUnsupported);
}

void CaptionBubble::LogSessionEvent(SessionEvent event) {
  if (model_ && !model_->HasError()) {
    base::UmaHistogramEnumeration("Accessibility.LiveCaption.Session2", event);
  }
}

std::vector<raw_ptr<views::View, VectorExperimental>>
CaptionBubble::GetButtons() {
  // TODO: the extraction here needs to be removed once the VectorExperimental
  // alias is removed.
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons = {
      back_to_tab_button_.get(), close_button_.get(), expand_button_.get(),
      collapse_button_.get(),    pin_button_.get(),   unpin_button_.get()};

  if (media::IsLiveTranslateEnabled() ||
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    buttons.push_back(language_label_.get());
  }

  return buttons;
}

bool CaptionBubble::HasActivity() {
  return model_ &&
         ((inactivity_timer_ && inactivity_timer_->IsRunning()) || HasFocus() ||
          !model_->GetFullText().empty() || model_->HasError() || is_pinned_ ||
          (media::IsLiveTranslateEnabled() &&
           download_progress_label_->GetVisible()));
}

views::Label* CaptionBubble::GetLabelForTesting() {
  return static_cast<views::Label*>(label_);
}

views::Label* CaptionBubble::GetDownloadProgressLabelForTesting() {
  return static_cast<views::Label*>(download_progress_label_);
}

views::Label* CaptionBubble::GetLanguageLabelForTesting() {
  return static_cast<views::Label*>(language_label_->GetLabel());
}

bool CaptionBubble::IsGenericErrorMessageVisibleForTesting() const {
  return generic_error_message_->GetVisible();
}

void CaptionBubble::SetCaptionBubbleStyle() {
  SetTextSizeAndFontFamily();
  if (GetWidget()) {
    SetTextColor();
    SetBackgroundColor();
    GetWidget()->ThemeChanged();
  }
}

base::RetainingOneShotTimer* CaptionBubble::GetInactivityTimerForTesting() {
  return inactivity_timer_.get();
}

views::Button* CaptionBubble::GetCloseButtonForTesting() {
  return close_button_.get();
}

views::Button* CaptionBubble::GetBackToTabButtonForTesting() {
  return back_to_tab_button_.get();
}

views::View* CaptionBubble::GetHeaderForTesting() {
  return header_container_.get();
}

void CaptionBubble::OnTitleTextChanged() {
  UpdateAccessibleName();
}

void CaptionBubble::UpdateAccessibleName() {
  GetViewAccessibility().SetName(title_->GetText());
}

BEGIN_METADATA(CaptionBubble)
END_METADATA

}  // namespace captions
