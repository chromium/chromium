// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/try_chrome_dialog_win/try_chrome_dialog.h"

#include <windows.h>

#include <shellapi.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/try_chrome_dialog_win/arrow_border.h"
#include "chrome/browser/ui/views/try_chrome_dialog_win/button_layout.h"
#include "chrome/browser/win/taskbar_icon_finder.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/installer/util/experiment.h"
#include "chrome/installer/util/experiment_storage.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace {

constexpr unsigned int kToastWidth = 360;
constexpr int kHoverAboveNotificationHeight = 24;

// The thickness of the border drawn around the inner edge of the popup.
constexpr int kTryChromeBorderThickness = 1;

// The space between the taskbar and the popup when it is positioned over an
// icon in the taskbar.
constexpr int kOffsetFromTaskbar = 1;

// The dimensions of the arrow image.
constexpr int kArrowWidth = 24;
constexpr int kArrowHeight = 14;

// The number of points the arrow icon is inset from the outer edge of the
// popup's border.
constexpr int kArrowInset = 3;

// Experiment specification information needed for layout.
struct ExperimentVariations {
  enum class CloseStyle {
    kNoThanksButton,
    kCloseX,
    kNoThanksButtonAndCloseX,
  };

  // Resource ID for header message string.
  int heading_id;
  // Resource ID for body message string, or 0 for no body text.
  int body_id;
  // Set of dismissal controls.
  CloseStyle close_style;
  // Which action to take on acceptance of the dialog.
  TryChromeDialog::Result result;
};

constexpr ExperimentVariations kExperiments[] = {
    {IDS_WIN10_TOAST_RECOMMENDATION, 0,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, 0,
     ExperimentVariations::CloseStyle::kNoThanksButtonAndCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, 0,
     ExperimentVariations::CloseStyle::kNoThanksButton,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, 0,
     ExperimentVariations::CloseStyle::kCloseX,
     // Was formerly OPEN_CHROME_WELCOME_WIN10 but that UI is deprecated,
     // so now kExperiments[3] == kExperiments[4].
     TryChromeDialog::OPEN_CHROME_WELCOME},
    {IDS_WIN10_TOAST_RECOMMENDATION, 0,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_WELCOME},
    {IDS_WIN10_TOAST_RECOMMENDATION, IDS_WIN10_TOAST_SWITCH_FAST,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, IDS_WIN10_TOAST_SWITCH_SECURE,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, IDS_WIN10_TOAST_SWITCH_SMART,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_FAST, IDS_WIN10_TOAST_RECOMMENDATION,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_SECURE, IDS_WIN10_TOAST_RECOMMENDATION,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_SMART, IDS_WIN10_TOAST_RECOMMENDATION,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_BROWSE_FAST, 0, ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_BROWSE_SAFELY, 0,
     ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_BROWSE_SMART, 0, ExperimentVariations::CloseStyle::kCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_SMART_AND_SECURE, IDS_WIN10_TOAST_RECOMMENDATION,
     ExperimentVariations::CloseStyle::kNoThanksButtonAndCloseX,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_SMART_AND_SECURE, IDS_WIN10_TOAST_RECOMMENDATION,
     ExperimentVariations::CloseStyle::kNoThanksButton,
     TryChromeDialog::OPEN_CHROME_DEFAULT}};

// Whether a button is an accept or cancel-style button.
enum class TryChromeButtonType { OPEN_CHROME, NO_THANKS };

}  // namespace

class Win10StyleButton : public views::LabelButton {
 public:
  METADATA_HEADER(Win10StyleButton);
  Win10StyleButton(views::Button::PressedCallback callback,
                   const std::u16string& text,
                   TryChromeButtonType button_type)
      : views::LabelButton(std::move(callback),
                           text,
                           CONTEXT_WINDOWS10_NATIVE) {
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetBackground(views::CreateThemedSolidBackground(
        button_type == TryChromeButtonType::OPEN_CHROME
            ? kColorTryChromeButtonBackgroundAccept
            : kColorTryChromeButtonBackgroundNoThanks));
    // Request specific 32pt height, 166+pt width.
    SetMinSize(gfx::Size(166, 32));
    SetMaxSize(gfx::Size(0, 32));
    // Make button focusable for keyboard navigation.
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  }
  Win10StyleButton(const Win10StyleButton&) = delete;
  Win10StyleButton& operator=(const Win10StyleButton&) = delete;
  ~Win10StyleButton() override = default;

  void OnThemeChanged() override {
    views::LabelButton::OnThemeChanged();
    SetEnabledTextColors(
        GetColorProvider()->GetColor(kColorTryChromeButtonForeground));
  }
};

BEGIN_VIEW_BUILDER(, Win10StyleButton, views::LabelButton)
END_VIEW_BUILDER

BEGIN_METADATA(Win10StyleButton, views::LabelButton)
END_METADATA

DEFINE_VIEW_BUILDER(, Win10StyleButton)

class BodyLabel : public views::Label {
 public:
  METADATA_HEADER(BodyLabel);
  BodyLabel() {
    SetTextContext(CONTEXT_WINDOWS10_NATIVE);
    SetMultiLine(true);
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  BodyLabel(const BodyLabel&) = delete;
  BodyLabel& operator=(const BodyLabel&) = delete;
  ~BodyLabel() override = default;

  // views::Label:
  void OnThemeChanged() override {
    views::Label::OnThemeChanged();
    SetBackgroundColor(GetColorProvider()->GetColor(kColorTryChromeBackground));
  }
};

BEGIN_VIEW_BUILDER(, BodyLabel, views::Label)
END_VIEW_BUILDER

BEGIN_METADATA(BodyLabel, views::Label)
END_METADATA

DEFINE_VIEW_BUILDER(, BodyLabel)

class HeadingLabel : public BodyLabel {
 public:
  METADATA_HEADER(HeadingLabel);
  HeadingLabel() = default;
  HeadingLabel(const HeadingLabel&) = delete;
  HeadingLabel& operator=(const HeadingLabel&) = delete;
  ~HeadingLabel() override = default;

  // BodyLabel:
  void OnThemeChanged() override {
    BodyLabel::OnThemeChanged();
    SetEnabledColor(
        GetColorProvider()->GetColor(kColorTryChromeHeaderForeground));
  }
};

BEGIN_VIEW_BUILDER(, HeadingLabel, views::Label)
END_VIEW_BUILDER

BEGIN_METADATA(HeadingLabel, views::Label)
END_METADATA

DEFINE_VIEW_BUILDER(, HeadingLabel)

// A View that unconditionally reports that it handles mouse presses. This
// results in the widget capturing the mouse so that it receives a
// ET_MOUSE_CAPTURE_CHANGED event upon button release following a drag out of
// the background of the widget.
class ClickableView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(ClickableView);
  ClickableView() = default;
  ClickableView(const ClickableView&) = delete;
  ClickableView& operator=(const ClickableView&) = delete;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
};

bool ClickableView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

BEGIN_VIEW_BUILDER(, ClickableView, views::BoxLayoutView)
END_VIEW_BUILDER

BEGIN_METADATA(ClickableView, views::BoxLayoutView)
END_METADATA

DEFINE_VIEW_BUILDER(, ClickableView)

// A helper class that determines properties of the desktop on which the popup
// will be shown, finds Chrome's taskbar icon, and handles calculations to
// position and draw the popup accordingly.
class TryChromeDialog::Context {
 public:
  Context();
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  // Suppresses use of the TaskbarIconFinder for the sake of tests. This omits
  // costly work that leads to flaky test failures.
  void BypassTaskbarIconSearch();

  // Begins asynchronous initialization of the context (i.e., runs a search for
  // the taskbar icon), running |closure| when done.
  void Initialize(base::OnceClosure closure);

  // Adds a border to |contents_view|, intended for use with |popup|. The
  // border's insets cover the area drawn by the border itself, including space
  // for the popup's arrow in case the popup is presented over a taskbar icon.
  void AddBorderToContents(views::Widget* popup, views::View* contents_view);

  // Computes the bouding rectangle of |popup| with the given |size| and applies
  // a shape to the popup's window as needed.
  gfx::Rect ComputePopupBounds(views::Widget* popup, const gfx::Size& size);

  // Returns the location where the popup will be presented.
  installer::ExperimentMetrics::ToastLocation GetToastLocation() const;

  // Returns the work area of the primary display (the one on which the popup
  // will be presented).
  const gfx::Rect& display_work_area() const {
    return primary_display_.work_area();
  }

  // Returns the bounding rectangle of the taskbar icon over which the popup is
  // to be presented.
  const gfx::Rect& taskbar_icon_rect() const { return taskbar_icon_rect_; }

 private:
  // An interface to a calclulator capable of drawing a border around a popup
  // and positioning it. Concrete subclasses of this handle positioning the
  // popup over the notification area or "over" the taskbar in any orientation.
  class DialogCalculator {
   public:
    DialogCalculator(const DialogCalculator&) = delete;
    DialogCalculator& operator=(const DialogCalculator&) = delete;
    virtual ~DialogCalculator() {}

    // Returns the ToastLocation metric to be reported when this calculator is
    // used.
    installer::ExperimentMetrics::ToastLocation toast_location() const {
      return toast_location_;
    }

    // Adds a border to |contents_view|, intended for use with |popup|. The
    // border's insets cover the area drawn by the border itself, including
    // space for the popup's arrow in case the popup is presented over a taskbar
    // icon.
    virtual void AddBorderToContents(views::Widget* popup,
                                     views::View* contents_view) = 0;

    // Returns the bounding rectangle of |popup| given the desired |size|,
    // shaping the window for |popup| as needed.
    virtual gfx::Rect ComputeBounds(const Context& context,
                                    views::Widget* popup,
                                    const gfx::Size& size) = 0;

   protected:
    explicit DialogCalculator(
        installer::ExperimentMetrics::ToastLocation toast_location)
        : toast_location_(toast_location) {}

   private:
    // The ToastLocation metric to be reported when this calculator is used.
    const installer::ExperimentMetrics::ToastLocation toast_location_;
  };

  // A calculator for positioning the popup over the notification area.
  class NotificationAreaCalculator : public DialogCalculator {
   public:
    NotificationAreaCalculator()
        : DialogCalculator(
              installer::ExperimentMetrics::kOverNotificationArea) {}
    NotificationAreaCalculator(const NotificationAreaCalculator&) = delete;
    NotificationAreaCalculator& operator=(const NotificationAreaCalculator&) =
        delete;

    // DialogCalculator:
    void AddBorderToContents(views::Widget* popup,
                             views::View* contents_view) override;
    gfx::Rect ComputeBounds(const Context& context,
                            views::Widget* popup,
                            const gfx::Size& size) override;
  };

  // A calculator for positioning the popup "over" Chrome's icon in the taskbar,
  // handling the four possible orientations of the taskbar. This includes
  // drawing the border and shaping the window for the arrow.
  class TaskbarCalculator : public DialogCalculator,
                            public views::WidgetObserver,
                            public views::WidgetRemovalsObserver {
   public:
    enum class Location { kTop, kLeft, kBottom, kRight };

    static std::unique_ptr<TaskbarCalculator> Create(Location location);

    TaskbarCalculator(const TaskbarCalculator&) = delete;
    TaskbarCalculator& operator=(const TaskbarCalculator&) = delete;
    ~TaskbarCalculator() override { CHECK(!IsInObserverList()); }

    // DialogCalculator:
    void AddBorderToContents(views::Widget* popup,
                             views::View* contents_view) override;
    gfx::Rect ComputeBounds(const Context& context,
                            views::Widget* popup,
                            const gfx::Size& size) override;

   private:
    // A pointer to a function that populates |polygon| with the seven points
    // that outline a popup at |dialog_bounds| within a window of |window_size|
    // containing an arrow at |arrow_bounds| (which defines the bounding
    // rectangle outside of the content region of the popup).
    // |arrow_border_insets| defines border-thickness insets into the arrow that
    // are used to properly define the region.
    using PopupRegionCreatorFn =
        void (*)(const gfx::Size& window_size,
                 const gfx::Rect& dialog_bounds,
                 const gfx::Rect& arrow_bounds,
                 const gfx::Insets& arrow_border_insets,
                 POINT* polygon);

    // Properties for an orientation-speciifc popup and its border.
    struct PopupProperties {
      // An inset that, when applied to the bounding rectangle of the arrow,
      // subtracts off the amount by which the arrow is set into the body of the
      // popup.
      const gfx::Insets arrow_inset;

      // The size of the arrow, taking into account any rotation needed (i.e., a
      // 24x14 arrow will be 14x24 after a 90 degree or 270 degree rotation).
      const gfx::Size arrow_size;

      // Indicates the direction to translate the arrow from the center of the
      // popup so that it is moved to the appripriate side of the popup (one of
      // (0,-1), (-1,0), (0,1), or (1,0)).
      const gfx::Vector2dF offset_scale;

      // The function that creates the proper region around the popup.
      PopupRegionCreatorFn region_creator;

      // Properties for the border around the popup and its arrow.
      const ArrowBorder::Properties border_properties;
    };

    // Creates a DialogCalculator for positioning the popup over a taskbar icon.
    explicit TaskbarCalculator(const PopupProperties* properties)
        : DialogCalculator(installer::ExperimentMetrics::kOverTaskbarPin),
          properties_(properties),
          contents_view_(nullptr),
          border_(nullptr) {}

    // views::WidgetObserver:
    // Updates the region defining the window shape of |popup|.
    void OnWidgetBoundsChanged(views::Widget* popup,
                               const gfx::Rect& new_bounds) override;

    // views::WidgetRemovalsObserver:
    void OnWillRemoveView(views::Widget* popup, views::View* view) override;

    // PopupRegionCreatorFn functions for the possible orientations.
    static void CreateTopArrowRegion(const gfx::Size& window_size,
                                     const gfx::Rect& dialog_bounds,
                                     const gfx::Rect& arrow_bounds,
                                     const gfx::Insets& arrow_border_insets,
                                     POINT* polygon);
    static void CreateLeftArrowRegion(const gfx::Size& window_size,
                                      const gfx::Rect& dialog_bounds,
                                      const gfx::Rect& arrow_bounds,
                                      const gfx::Insets& arrow_border_insets,
                                      POINT* polygon);
    static void CreateBottomArrowRegion(const gfx::Size& window_size,
                                        const gfx::Rect& dialog_bounds,
                                        const gfx::Rect& arrow_bounds,
                                        const gfx::Insets& arrow_border_insets,
                                        POINT* polygon);
    static void CreateRightArrowRegion(const gfx::Size& window_size,
                                       const gfx::Rect& dialog_bounds,
                                       const gfx::Rect& arrow_bounds,
                                       const gfx::Insets& arrow_border_insets,
                                       POINT* polygon);

    // Popup properties for the possible orientations.
    static const PopupProperties kTopTaskbarProperties_;
    static const PopupProperties kLeftTaskbarProperties_;
    static const PopupProperties kBottomTaskbarProperties_;
    static const PopupProperties kRightTaskbarProperties_;

    const raw_ptr<const PopupProperties> properties_;

    // A horizontal (for top/bottom taskbars) or vertical (for left/right)
    // displacement, in DIP, for the arrow to keep it centered with respect to
    // the taskbar icon.
    gfx::Vector2d arrow_adjustment_;

    raw_ptr<views::View> contents_view_;
    raw_ptr<ArrowBorder> border_;

    // The last size, in pixels, for the popup's window for which its region was
    // calculated.
    gfx::Size window_size_;
  };

  enum class TaskbarLocation { kUnknown, kTop, kLeft, kBottom, kRight };

  // Returns the window of the taskbar on the primary display.
  static HWND FindTaskbarWindow();

  // Returns the bounding rectangle of |taskbar_window| or an empty rect if
  // |taskbar_window| is invalid or its bounds cannot be determined.
  static gfx::Rect GetTaskbarRect(HWND taskbar_window);

  // Returns the location of the taskbar on |primary_display| given
  // |taskbar_rect| as its bounding rectangle. Returns TaskbarLocation::kUnknown
  // if |taskbar_rect| is empty, the taskbar is hidden, or its location cannot
  // be determined for any other reason.
  static TaskbarLocation FindTaskbarLocation(
      const display::Display& primary_display,
      const gfx::Rect& taskbar_rect);

  // Receives the bounding recangle of Chrome's taskbar icon, configures the
  // instance accordingly, and continues processing by running |closure| (as
  // provided to Initialize).
  void OnTaskbarIconRect(base::OnceClosure closure,
                         const gfx::Rect& taskbar_icon_rect);

  // Returns a Calculator instance for presentation of the popup based on the
  // presence and position of the taskbar icon.
  std::unique_ptr<DialogCalculator> MakeCalculator() const;

  // The primary display.
  const display::Display primary_display_;

  // The window of the taskbar on the primary display, or null.
  const HWND taskbar_window_;

  // The bounding rectangle of the taskbar on the primary display, or an empty
  // rect.
  const gfx::Rect taskbar_rect_;

  // The location of the taskbar on the primary display, or kUnknown.
  const TaskbarLocation taskbar_location_;

  bool bypass_taskbar_icon_search_ = false;

  // The bounding rectangle of Chrome's icon in the primary taskbar.
  gfx::Rect taskbar_icon_rect_;

  // A dialog calculator to position and draw the popup based on the presence
  // and location of the taskbar icon.
  std::unique_ptr<DialogCalculator> calculator_;
};

// TryChromeDialog::Context::Context -------------------------------------------

TryChromeDialog::Context::Context()
    : primary_display_(display::Screen::GetScreen()->GetPrimaryDisplay()),
      taskbar_window_(FindTaskbarWindow()),
      taskbar_rect_(GetTaskbarRect(taskbar_window_)),
      taskbar_location_(FindTaskbarLocation(primary_display_, taskbar_rect_)) {}

void TryChromeDialog::Context::BypassTaskbarIconSearch() {
  bypass_taskbar_icon_search_ = true;
}

void TryChromeDialog::Context::Initialize(base::OnceClosure closure) {
  auto result_callback =
      base::BindOnce(&TryChromeDialog::Context::OnTaskbarIconRect,
                     base::Unretained(this), std::move(closure));

  if (bypass_taskbar_icon_search_) {
    // When testing, skip the search for the taskbar icon and simply proceed
    // with an empty rect indicating that no taskbar icon was found.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback), gfx::Rect()));
    return;
  }

  // Get the bounding rectangle of Chrome's taskbar icon on the primary monitor.
  FindTaskbarIcon(std::move(result_callback));
}

void TryChromeDialog::Context::AddBorderToContents(views::Widget* popup,
                                                   views::View* contents_view) {
  calculator_->AddBorderToContents(popup, contents_view);
}

gfx::Rect TryChromeDialog::Context::ComputePopupBounds(views::Widget* popup,
                                                       const gfx::Size& size) {
  return calculator_->ComputeBounds(*this, popup, size);
}

installer::ExperimentMetrics::ToastLocation
TryChromeDialog::Context::GetToastLocation() const {
  return calculator_->toast_location();
}

// static
HWND TryChromeDialog::Context::FindTaskbarWindow() {
  return ::FindWindow(L"Shell_TrayWnd", nullptr);
}

// static
gfx::Rect TryChromeDialog::Context::GetTaskbarRect(HWND taskbar_window) {
  RECT temp_rect = {};
  if (!taskbar_window || !::GetWindowRect(taskbar_window, &temp_rect))
    return gfx::Rect();
  return display::win::ScreenWin::ScreenToDIPRect(taskbar_window,
                                                  gfx::Rect(temp_rect));
}

// static
TryChromeDialog::Context::TaskbarLocation
TryChromeDialog::Context::FindTaskbarLocation(
    const display::Display& primary_display,
    const gfx::Rect& taskbar_rect) {
  if (taskbar_rect.IsEmpty())
    return TaskbarLocation::kUnknown;

  // The taskbar is always on the primary display.
  const gfx::Rect& monitor_rect = primary_display.bounds();

  // Is the taskbar not on the primary display (e.g., is it hidden)?
  if (!monitor_rect.Contains(taskbar_rect))
    return TaskbarLocation::kUnknown;

  // Where is the taskbar? Assume that it's "wider" than it is "tall".
  if (taskbar_rect.width() > taskbar_rect.height()) {
    // Horizonal.
    if (taskbar_rect.y() >= monitor_rect.y() + monitor_rect.height() / 2)
      return TaskbarLocation::kBottom;
    return TaskbarLocation::kTop;
  }
  // Vertical.
  if (taskbar_rect.x() < monitor_rect.x() + monitor_rect.width() / 2)
    return TaskbarLocation::kLeft;
  return TaskbarLocation::kRight;
}

void TryChromeDialog::Context::OnTaskbarIconRect(
    base::OnceClosure closure,
    const gfx::Rect& taskbar_icon_rect) {
  taskbar_icon_rect_ = taskbar_icon_rect;
  calculator_ = MakeCalculator();
  std::move(closure).Run();
}

std::unique_ptr<TryChromeDialog::Context::DialogCalculator>
TryChromeDialog::Context::MakeCalculator() const {
  TaskbarLocation location = taskbar_location_;

  // Present the popup over the notification area if the taskbar couldn't be
  // found, the icon couldn't be found, or the taskbar doesn't contain the icon
  // (e.g., the icon is scrolled out of view).
  if (taskbar_icon_rect_.IsEmpty() ||
      (location != TaskbarLocation::kUnknown &&
       !taskbar_rect_.Contains(taskbar_icon_rect_))) {
    location = TaskbarLocation::kUnknown;
  }

  switch (location) {
    case TaskbarLocation::kUnknown:
      break;
    case TaskbarLocation::kTop:
      return TaskbarCalculator::Create(TaskbarCalculator::Location::kTop);
    case TaskbarLocation::kLeft:
      return TaskbarCalculator::Create(TaskbarCalculator::Location::kLeft);
    case TaskbarLocation::kBottom:
      return TaskbarCalculator::Create(TaskbarCalculator::Location::kBottom);
    case TaskbarLocation::kRight:
      return TaskbarCalculator::Create(TaskbarCalculator::Location::kRight);
  }
  return std::make_unique<NotificationAreaCalculator>();
}

// TryChromeDialog::Context::NotificationAreaCalculator ------------------------

void TryChromeDialog::Context::NotificationAreaCalculator::AddBorderToContents(
    views::Widget* popup,
    views::View* contents_view) {
  contents_view->SetBorder(views::CreateThemedSolidBorder(
      kTryChromeBorderThickness, kColorTryChromeBorder));
}

gfx::Rect TryChromeDialog::Context::NotificationAreaCalculator::ComputeBounds(
    const Context& context,
    views::Widget* popup,
    const gfx::Size& size) {
  const bool is_RTL = base::i18n::IsRTL();
  const gfx::Rect work_area = popup->GetWorkAreaBoundsInScreen();
  return gfx::Rect(
      is_RTL ? work_area.x() : work_area.right() - size.width(),
      work_area.bottom() - size.height() - kHoverAboveNotificationHeight,
      size.width(), size.height());
}

// TryChromeDialog::Context::TaskbarCalculator ---------------------------------

std::unique_ptr<TryChromeDialog::Context::TaskbarCalculator>
TryChromeDialog::Context::TaskbarCalculator::Create(Location location) {
  switch (location) {
    case Location::kTop:
      return base::WrapUnique(new TaskbarCalculator(&kTopTaskbarProperties_));
    case Location::kLeft:
      return base::WrapUnique(new TaskbarCalculator(&kLeftTaskbarProperties_));
    case Location::kBottom:
      break;
    case Location::kRight:
      return base::WrapUnique(new TaskbarCalculator(&kRightTaskbarProperties_));
  }
  return base::WrapUnique(new TaskbarCalculator(&kBottomTaskbarProperties_));
}

void TryChromeDialog::Context::TaskbarCalculator::AddBorderToContents(
    views::Widget* popup,
    views::View* contents_view) {
  // Hold a pointer to the border so that it can be given the exact position of
  // the arrow after the popup is sized and placed at the proper screen
  // location. Also hold a pointer to the view to which the border is attached
  // and observe the popup so that these pointers can be appropriately cleared.
  contents_view_ = contents_view;
  auto border = std::make_unique<ArrowBorder>(
      kTryChromeBorderThickness, kColorTryChromeBorder,
      kColorTryChromeBackground, kInactiveToastArrowIcon,
      &properties_->border_properties);
  border_ = border.get();
  contents_view->SetBorder(std::move(border));
  popup->AddObserver(this);
  popup->AddRemovalsObserver(this);
}

gfx::Rect TryChromeDialog::Context::TaskbarCalculator::ComputeBounds(
    const Context& context,
    views::Widget* popup,
    const gfx::Size& size) {
  // Center the popup over the icon.
  const gfx::RectF taskbar_icon_rect(context.taskbar_icon_rect());
  gfx::PointF popup_origin(taskbar_icon_rect.CenterPoint());
  popup_origin.Offset(size.width() / -2.0f, size.height() / -2.0f);

  // Move the popup away from the center of the taskbar icon by the
  // orientation-specific offset. This will move it along one of the axes to
  // push the popup up (for a bottm-of-screen taskbar), to the right right (for
  // a left-of-screen taskbar), etc.
  gfx::Vector2dF translation(
      (taskbar_icon_rect.width() + size.width()) / 2.0f + kOffsetFromTaskbar,
      (taskbar_icon_rect.height() + size.height()) / 2.0f + kOffsetFromTaskbar);

  // offset_scale clears out one axis and makes the other positive or negative,
  // as appropriate.
  translation.Scale(properties_->offset_scale.x(),
                    properties_->offset_scale.y());
  popup_origin += translation;

  const gfx::Rect desired_bounds(gfx::ToRoundedPoint(popup_origin), size);

  // Adjust the popup to fit in the work area to handle the case where the icon
  // is close to the edge.
  gfx::Rect result = desired_bounds;
  result.AdjustToFit(context.display_work_area());

  // Remember the amount of offset for proper arrow placement.
  arrow_adjustment_ = result.origin() - desired_bounds.origin();
  if (properties_->offset_scale.x())  // Popup is next to the icon.
    arrow_adjustment_.set_x(0);
  else  // Popup is over/under the icon.
    arrow_adjustment_.set_y(0);

  return result;
}

void TryChromeDialog::Context::TaskbarCalculator::OnWidgetBoundsChanged(
    views::Widget* popup,
    const gfx::Rect& new_bounds) {
  // Compute and apply the region to shape the dialog around the arrow. The
  // region must be in screen rather than DIP coordinates relative to the
  // client area of the window.

  aura::WindowTreeHost* const host = popup->GetNativeView()->GetHost();
  // Nothing to do if there's not yet a backing window.
  if (!host)
    return;

  // Nothing to do if the contents have yet to be added.
  if (!popup->GetContentsView())
    return;

  // Get the new window size.
  const gfx::Size window_size = host->GetBoundsInPixels().size();

  // Nothing to do if the size of the window hasn't changed since a previous
  // region calculation.
  if (window_size == window_size_)
    return;

  // Remember this size for the future and to protect from a chain of
  // recursive calls when ::SetWindowRgn is called below.
  window_size_ = window_size;

  const HWND hwnd = host->GetAcceleratedWidget();
  const float dsf = display::win::ScreenWin::GetScaleFactorForHWND(hwnd);

  // Compute the pixel position of the arrow relative to the window's client
  // area.

  // Center the arrow over the window.
  gfx::SizeF arrow_size(properties_->arrow_size);
  arrow_size.Scale(dsf);
  gfx::PointF arrow_origin(window_size.width() / 2.0f,
                           window_size.height() / 2.0f);
  arrow_origin.Offset(arrow_size.width() / -2.0f, arrow_size.height() / -2.0f);

  // Push it out to its proper location.
  gfx::Vector2dF translation(
      (arrow_size.width() + window_size.width()) / 2.0f,
      (arrow_size.height() + window_size.height()) / 2.0f);
  translation.Scale(properties_->offset_scale.x(),
                    properties_->offset_scale.y());
  arrow_origin -= translation;

  // Select the bounding rectangle by putting the origin on the nearest point
  // and rouding the size.
  gfx::Rect arrow_bounds(gfx::ToRoundedPoint(arrow_origin),
                         gfx::ToRoundedSize(arrow_size));

  // Move it inward so that it is flush with the outer edge of the window.
  arrow_bounds.AdjustToFit(gfx::Rect(window_size));

  // Offset by the adjustment from ComputeBounds to keep it centered with
  // respect to the taskbar icon.
  arrow_bounds -= gfx::ToRoundedVector2d(
      gfx::ScaleVector2d(gfx::Vector2dF(arrow_adjustment_), dsf));

  // Tell the border about the new location for the arrow so that it is painted
  // in the correct place.
  border_->set_arrow_bounds(arrow_bounds);

  // Compute the bounding rectangle of the dialog (the visible rect including
  // the border without the arrow).
  const gfx::Insets border_insets_in_pixels = gfx::ToFlooredInsets(
      gfx::ConvertInsetsToPixels(gfx::Insets(kTryChromeBorderThickness), dsf));
  gfx::Insets scaled_insets = gfx::ToFlooredInsets(
      gfx::ConvertInsetsToPixels(popup->GetContentsView()->GetInsets(), dsf));
  scaled_insets -= border_insets_in_pixels;
  gfx::Rect dialog_bounds(window_size);
  dialog_bounds.Inset(scaled_insets);

  // Clip the arrow to the bounds of the dialog.
  arrow_bounds.Subtract(dialog_bounds);

  // Scale the insets into the arrow's bounding rectangle that the border
  // extends into it.
  gfx::Insets arrow_border_insets =
      gfx::ToFlooredInsets(gfx::ConvertInsetsToPixels(
          properties_->border_properties.arrow_border_insets, dsf));

  POINT polygon[7];
  properties_->region_creator(window_size, dialog_bounds, arrow_bounds,
                              arrow_border_insets, &polygon[0]);
  HRGN region = ::CreatePolygonRgn(&polygon[0], std::size(polygon), WINDING);
  ::SetWindowRgn(hwnd, region, FALSE);
}

void TryChromeDialog::Context::TaskbarCalculator::OnWillRemoveView(
    views::Widget* popup,
    views::View* view) {
  if (view == contents_view_) {
    // The view to which the border was added is being removed. Clear out the
    // weak pointers to the view and border (which is owned by the view) and
    // stop observing the widget.
    contents_view_ = nullptr;
    border_ = nullptr;
    popup->RemoveRemovalsObserver(this);
    popup->RemoveObserver(this);
  }
}

// static
void TryChromeDialog::Context::TaskbarCalculator::CreateTopArrowRegion(
    const gfx::Size& window_size,
    const gfx::Rect& dialog_bounds,
    const gfx::Rect& arrow_bounds,
    const gfx::Insets& arrow_border_insets,
    POINT* polygon) {
  polygon[0] = {dialog_bounds.x(), dialog_bounds.y()};
  polygon[1] = {arrow_bounds.x() + arrow_border_insets.left(),
                dialog_bounds.y()};
  polygon[2] = {arrow_bounds.x() + arrow_bounds.width() / 2, 0};
  polygon[3] = {arrow_bounds.right() - arrow_border_insets.right(),
                dialog_bounds.y()};
  polygon[4] = {dialog_bounds.right(), dialog_bounds.y()};
  polygon[5] = {dialog_bounds.right(), dialog_bounds.bottom()};
  polygon[6] = {dialog_bounds.x(), dialog_bounds.bottom()};
}

// static
void TryChromeDialog::Context::TaskbarCalculator::CreateLeftArrowRegion(
    const gfx::Size& window_size,
    const gfx::Rect& dialog_bounds,
    const gfx::Rect& arrow_bounds,
    const gfx::Insets& arrow_border_insets,
    POINT* polygon) {
  polygon[0] = {dialog_bounds.x(), dialog_bounds.y()};
  polygon[1] = {dialog_bounds.right(), dialog_bounds.y()};
  polygon[2] = {dialog_bounds.right(), dialog_bounds.bottom()};
  polygon[3] = {dialog_bounds.x(), dialog_bounds.bottom()};
  polygon[4] = {dialog_bounds.x(),
                arrow_bounds.bottom() - arrow_border_insets.bottom()};
  polygon[5] = {0, arrow_bounds.y() + arrow_bounds.height() / 2};
  polygon[6] = {dialog_bounds.x(),
                arrow_bounds.y() + arrow_border_insets.top()};
}

// static
void TryChromeDialog::Context::TaskbarCalculator::CreateBottomArrowRegion(
    const gfx::Size& window_size,
    const gfx::Rect& dialog_bounds,
    const gfx::Rect& arrow_bounds,
    const gfx::Insets& arrow_border_insets,
    POINT* polygon) {
  polygon[0] = {dialog_bounds.x(), dialog_bounds.y()};
  polygon[1] = {dialog_bounds.right(), dialog_bounds.y()};
  polygon[2] = {dialog_bounds.right(), dialog_bounds.bottom()};
  polygon[3] = {arrow_bounds.right() - arrow_border_insets.right(),
                dialog_bounds.bottom()};
  polygon[4] = {arrow_bounds.x() + arrow_bounds.width() / 2,
                window_size.height()};
  polygon[5] = {arrow_bounds.x() + arrow_border_insets.left(),
                dialog_bounds.bottom()};
  polygon[6] = {dialog_bounds.x(), dialog_bounds.bottom()};
}

// static
void TryChromeDialog::Context::TaskbarCalculator::CreateRightArrowRegion(
    const gfx::Size& window_size,
    const gfx::Rect& dialog_bounds,
    const gfx::Rect& arrow_bounds,
    const gfx::Insets& arrow_border_insets,
    POINT* polygon) {
  polygon[0] = {dialog_bounds.x(), dialog_bounds.y()};
  polygon[1] = {dialog_bounds.right(), dialog_bounds.y()};
  polygon[2] = {dialog_bounds.right(),
                arrow_bounds.y() + arrow_border_insets.top()};
  polygon[3] = {window_size.width(),
                arrow_bounds.y() + arrow_bounds.height() / 2};
  polygon[4] = {dialog_bounds.right(),
                arrow_bounds.bottom() - arrow_border_insets.bottom()};
  polygon[5] = {dialog_bounds.right(), dialog_bounds.bottom()};
  polygon[6] = {dialog_bounds.x(), dialog_bounds.bottom()};
}

// static
constexpr TryChromeDialog::Context::TaskbarCalculator::PopupProperties
    TryChromeDialog::Context::TaskbarCalculator::kTopTaskbarProperties_ = {
        gfx::Insets::TLBR(0, 0, kArrowInset, 0),
        {kArrowWidth, kArrowHeight},
        {0.0f, 1.0f} /* Translate down */,
        &CreateTopArrowRegion,
        {// ArrowBorder::Properties
         gfx::Insets::TLBR(kArrowHeight - kArrowInset, 0, 0, 0),
         gfx::Insets::TLBR(0,
                           kTryChromeBorderThickness,
                           0,
                           kTryChromeBorderThickness),
         ArrowBorder::ArrowRotation::k180Degrees}};

// static
constexpr TryChromeDialog::Context::TaskbarCalculator::PopupProperties
    TryChromeDialog::Context::TaskbarCalculator::kLeftTaskbarProperties_{
        gfx::Insets::TLBR(0, 0, 0, kArrowInset),
        {kArrowHeight, kArrowWidth},
        {1.0f, 0.0f} /* Translate right */,
        &CreateLeftArrowRegion,
        {// ArrowBorder::Properties
         gfx::Insets::TLBR(0, kArrowHeight - kArrowInset, 0, 0),
         gfx::Insets::TLBR(kTryChromeBorderThickness,
                           0,
                           kTryChromeBorderThickness,
                           0),
         ArrowBorder::ArrowRotation::k90Degrees}};

// static
constexpr TryChromeDialog::Context::TaskbarCalculator::PopupProperties
    TryChromeDialog::Context::TaskbarCalculator::kBottomTaskbarProperties_{
        gfx::Insets::TLBR(kArrowInset, 0, 0, 0),
        {kArrowWidth, kArrowHeight},
        {0.0f, -1.0f} /* Translate up */,
        &CreateBottomArrowRegion,
        {// ArrowBorder::Properties
         gfx::Insets::TLBR(0, 0, kArrowHeight - kArrowInset, 0),
         gfx::Insets::TLBR(0,
                           kTryChromeBorderThickness,
                           0,
                           kTryChromeBorderThickness),
         ArrowBorder::ArrowRotation::kNone}};

// static
constexpr TryChromeDialog::Context::TaskbarCalculator::PopupProperties
    TryChromeDialog::Context::TaskbarCalculator::kRightTaskbarProperties_{
        gfx::Insets::TLBR(0, kArrowInset, 0, 0),
        {kArrowHeight, kArrowWidth},
        {-1.0f, 0.0f} /* Translate left */,
        &CreateRightArrowRegion,
        {// ArrowBorder::Properties
         gfx::Insets::TLBR(0, 0, 0, kArrowHeight - kArrowInset),
         gfx::Insets::TLBR(kTryChromeBorderThickness,
                           0,
                           kTryChromeBorderThickness,
                           0),
         ArrowBorder::ArrowRotation::k270Degrees}};

// TryChromeDialog::ModalShowDelegate ------------------------------------------

// A delegate for use by the modal Show() function to update the experiment
// state in the Windows registry and break out of the modal run loop upon
// completion.
class TryChromeDialog::ModalShowDelegate : public TryChromeDialog::Delegate {
 public:
  // Constructs the updater with a closure to be run after the dialog is closed
  // to break out of the modal run loop.
  explicit ModalShowDelegate(base::RepeatingClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}
  ModalShowDelegate(const ModalShowDelegate&) = delete;
  ModalShowDelegate& operator=(const ModalShowDelegate&) = delete;
  ~ModalShowDelegate() override = default;

 protected:
  // TryChromeDialog::Delegate:
  void SetToastLocation(
      installer::ExperimentMetrics::ToastLocation toast_location) override;
  void SetExperimentState(installer::ExperimentMetrics::State state) override;
  void InteractionComplete() override;

 private:
  base::RepeatingClosure quit_closure_;
  installer::ExperimentStorage storage_;

  // The time at which the toast was shown; used for computing the action delay.
  base::TimeTicks time_shown_;
};

void TryChromeDialog::ModalShowDelegate::SetToastLocation(
    installer::ExperimentMetrics::ToastLocation toast_location) {
  time_shown_ = base::TimeTicks::Now();

  installer::Experiment experiment;
  auto lock = storage_.AcquireLock();
  if (lock->LoadExperiment(&experiment)) {
    experiment.SetDisplayTime(base::Time::Now());
    experiment.SetToastCount(experiment.toast_count() + 1);
    experiment.SetToastLocation(toast_location);
    // TODO(skare): SetUserSessionUptime
    lock->StoreExperiment(experiment);
  }
}

void TryChromeDialog::ModalShowDelegate::SetExperimentState(
    installer::ExperimentMetrics::State state) {
  installer::Experiment experiment;
  auto lock = storage_.AcquireLock();
  if (lock->LoadExperiment(&experiment)) {
    if (!time_shown_.is_null())
      experiment.SetActionDelay(base::TimeTicks::Now() - time_shown_);
    experiment.SetState(state);
    lock->StoreExperiment(experiment);
  }
}

void TryChromeDialog::ModalShowDelegate::InteractionComplete() {
  quit_closure_.Run();
}

// TryChromeDialog -------------------------------------------------------------

// static
TryChromeDialog::Result TryChromeDialog::Show(
    size_t group,
    ActiveModalDialogListener listener) {
  if (group >= std::size(kExperiments)) {
    // Exit immediately given bogus values; see TryChromeDialogBrowserTest test.
    return NOT_NOW;
  }

  base::RunLoop run_loop;
  ModalShowDelegate delegate(run_loop.QuitWhenIdleClosure());
  TryChromeDialog dialog(group, &delegate);

  dialog.ShowDialogAsync();

  if (listener) {
    listener.Run(base::BindRepeating(&TryChromeDialog::OnProcessNotification,
                                     base::Unretained(&dialog)));
  }
  run_loop.Run();
  if (listener)
    listener.Run(base::NullCallback());

  return dialog.result();
}

TryChromeDialog::TryChromeDialog(size_t group, Delegate* delegate)
    : group_(group),
      delegate_(delegate),
      context_(std::make_unique<Context>()) {
  DCHECK_LT(group, std::size(kExperiments));
  DCHECK(delegate);
}

TryChromeDialog::~TryChromeDialog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  CHECK(!IsInObserverList());
}

void TryChromeDialog::BypassTaskbarIconSearchForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  context_->BypassTaskbarIconSearch();
}

void TryChromeDialog::ShowDialogAsync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  endsession_observer_ =
      std::make_unique<gfx::SingletonHwndObserver>(base::BindRepeating(
          &TryChromeDialog::OnWindowMessage, base::Unretained(this)));

  context_->Initialize(base::BindOnce(&TryChromeDialog::OnContextInitialized,
                                      base::Unretained(this)));
}

void TryChromeDialog::OnContextInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  // It's possible that a rendezvous from another browser process arrived while
  // searching for the taskbar icon (see OnProcessNotification). In this case,
  // report the DEFER result and bail out immediately.
  if (result_ == OPEN_CHROME_DEFER) {
    DCHECK_EQ(state_, installer::ExperimentMetrics::kOtherLaunch);
    CompleteInteraction();
    return;
  }

  // It's also possible that a WM_ENDSESSION arrived while searching (see
  // OnWindowMessage). In this case, continue processing since it's possible
  // that the logoff was cancelled. The toast may as well be shown.

  // Create the popup.
  auto logo = views::Builder<views::ImageView>()
                  .SetImage(ui::ImageModel::FromVectorIcon(
                      kInactiveToastLogoIcon, kColorTryChromeHeaderForeground))
                  .Build();
  const gfx::Size logo_size = logo->GetPreferredSize();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  // An approximate window size. Layout() can adjust.
  params.bounds = gfx::Rect(kToastWidth, 120);
  params.name = "TryChromeDialog";
  popup_ = new views::Widget;
  popup_->AddObserver(this);
  popup_->Init(std::move(params));

  auto contents_view =
      views::Builder<ClickableView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetMinimumCrossAxisSize(kToastWidth)
          .CustomConfigure(base::BindOnce(
              [](Context* context, views::Widget* popup,
                 ClickableView* contents_view) {
                contents_view->SetBackground(views::CreateThemedSolidBackground(
                    kColorTryChromeBackground));
                context->AddBorderToContents(popup, contents_view);
              },
              context_.get(), popup_));

  // Padding around the left, top, and right of the logo.
  static constexpr int kLogoPadding = 10;
  static constexpr int kCloseButtonWidth = 24;
  static constexpr int kCloseButtonTopPadding = 6;
  static constexpr int kCloseButtonRightPadding = 5;
  static constexpr int kSpacingAfterHeadingHorizontal = 40;
  static constexpr int kSpacingHeadingToClose = kSpacingAfterHeadingHorizontal -
                                                kCloseButtonWidth -
                                                kCloseButtonRightPadding;

  // Padding around all sides of the text buttons (but not between them).
  static constexpr int kTextButtonPadding = 12;

  // First two rows: [pad][logo][pad][text][pad][close button].
  // Only the close button is in the first row, spanning both. The logo and main
  // header are in the second row.
  const int kLabelWidth = kToastWidth - kLogoPadding - logo_size.width() -
                          kLogoPadding - kSpacingAfterHeadingHorizontal;

  auto table_view =
      views::Builder<views::TableLayoutView>()
          .SetIncludeHidden(true)
          .AddPaddingColumn(views::TableLayout::kFixedSize,
                            kLogoPadding - kTryChromeBorderThickness)
          .AddColumn(views::LayoutAlignment::kStart,
                     views::LayoutAlignment::kStart,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kFixed, logo_size.width(),
                     logo_size.height())
          .AddPaddingColumn(views::TableLayout::kFixedSize, kLogoPadding)
          .AddColumn(views::LayoutAlignment::kStretch,
                     views::LayoutAlignment::kStretch, 1.0,
                     views::TableLayout::ColumnSize::kFixed, kLabelWidth, 0)
          .AddPaddingColumn(views::TableLayout::kFixedSize,
                            kSpacingHeadingToClose)
          .AddColumn(views::LayoutAlignment::kStart,
                     views::LayoutAlignment::kStart,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .AddPaddingColumn(
              views::TableLayout::kFixedSize,
              kCloseButtonRightPadding - kTryChromeBorderThickness)
          .AddPaddingRow(views::TableLayout::kFixedSize,
                         kCloseButtonTopPadding - kTryChromeBorderThickness)
          .AddRows(1, views::TableLayout::kFixedSize,
                   kLogoPadding - kCloseButtonTopPadding)
          .AddRows(1, views::TableLayout::kFixedSize, 0)
          // Skip two columns in this first row
          .AddChildren(views::Builder<views::View>(),
                       views::Builder<views::View>());

  // Close button if included in the variant.
  if (kExperiments[group_].close_style ==
          ExperimentVariations::CloseStyle::kCloseX ||
      kExperiments[group_].close_style ==
          ExperimentVariations::CloseStyle::kNoThanksButtonAndCloseX) {
    table_view.AddChild(
        views::Builder<views::ImageButton>()
            .CopyAddressTo(&close_button_)
            .SetCallback(base::BindRepeating(
                &TryChromeDialog::ButtonPressed, base::Unretained(this),
                installer::ExperimentMetrics::kSelectedClose))
            .SetVisible(false)
            // TODO(crbug.com/1218186): Check this. This is in place to pass
            // accessibility checks.
            .SetAccessibleName(l10n_util::GetStringUTF16(IDS_CLOSE))
            .SetProperty(views::kTableColAndRowSpanKey, gfx::Size(1, 2))
            .CustomConfigure(
                base::BindOnce([](views::ImageButton* image_button) {
                  image_button->SetImageModel(
                      views::Button::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          kInactiveToastCloseIcon, kColorTryChromeForeground));
                  DCHECK_EQ(image_button->GetPreferredSize().width(),
                            kCloseButtonWidth);
                })));
  } else {
    table_view.AddChild(views::Builder<views::View>().SetProperty(
        views::kTableColAndRowSpanKey, gfx::Size(1, 2)));
  }

  table_view.AddChildren(
      views::Builder<views::ImageView>(std::move(logo)),
      views::Builder<HeadingLabel>()
          .SetText(l10n_util::GetStringUTF16(kExperiments[group_].heading_id)));

  contents_view.AddChild(std::move(table_view));

  // Optional third row: [pad][text].
  // Third row: May have text or may be blank.
  const int body_string_id = kExperiments[group_].body_id;
  if (body_string_id) {
    const int logo_padding = logo_size.width() + kLogoPadding;
    contents_view.AddChild(
        views::Builder<BodyLabel>()
            .SetText(l10n_util::GetStringUTF16(body_string_id))
            .SetProperty(
                views::kMarginsKey,
                gfx::Insets::TLBR(
                    0, kLogoPadding - kTryChromeBorderThickness + logo_padding,
                    0, 0)));
  }

  //             [      pad        ].
  // Fourth row: [pad][buttons][pad].
  //             [      pad        ].
  // One or two buttons depending on group.
  static constexpr int kButtonsViewWidth =
      kToastWidth - kTextButtonPadding - kTextButtonPadding;
  auto buttons =
      views::Builder<views::View>()
          .SetLayoutManager(std::make_unique<ButtonLayout>(kButtonsViewWidth))
          .AddChild(views::Builder<
                    Win10StyleButton>(std::make_unique<Win10StyleButton>(
              base::BindRepeating(
                  &TryChromeDialog::ButtonPressed, base::Unretained(this),
                  installer::ExperimentMetrics::kSelectedOpenChromeAndNoCrash),
              l10n_util::GetStringUTF16(IDS_WIN10_TOAST_OPEN_CHROME),
              TryChromeButtonType::OPEN_CHROME)));
  const auto style = kExperiments[group_].close_style;
  if (style == ExperimentVariations::CloseStyle::kNoThanksButton ||
      style == ExperimentVariations::CloseStyle::kNoThanksButtonAndCloseX) {
    buttons.AddChild(views::Builder<
                    Win10StyleButton>(std::make_unique<Win10StyleButton>(
        base::BindRepeating(&TryChromeDialog::ButtonPressed,
                            base::Unretained(this),
                            installer::ExperimentMetrics::kSelectedNoThanks),
        l10n_util::GetStringUTF16(IDS_WIN10_TOAST_NO_THANKS),
        TryChromeButtonType::NO_THANKS)));
  }

  contents_view.AddChild(
      views::Builder<views::TableLayoutView>()
          .AddPaddingColumn(views::TableLayout::kFixedSize,
                            kTextButtonPadding - kTryChromeBorderThickness)
          .AddColumn(views::LayoutAlignment::kStretch,
                     views::LayoutAlignment::kStretch,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .AddPaddingColumn(views::TableLayout::kFixedSize,
                            kTextButtonPadding - kTryChromeBorderThickness)
          .AddPaddingRow(views::TableLayout::kFixedSize, kTextButtonPadding)
          .AddRows(1, views::TableLayout::kFixedSize, 0)
          .AddPaddingRow(views::TableLayout::kFixedSize,
                         kTextButtonPadding - kTryChromeBorderThickness)
          .AddChild(std::move(buttons)));

  popup_->SetContentsView(std::move(contents_view).Build());

  // Compute the preferred size after attaching the contents view to the popup,
  // as doing such causes the theme to propagate through the view hierarchy.
  // This propagation can cause views to change their size requirements.
  const gfx::Size preferred = popup_->GetContentsView()->GetPreferredSize();
  popup_->SetBounds(context_->ComputePopupBounds(popup_, preferred));
  popup_->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);

  popup_->ShowInactive();
  delegate_->SetToastLocation(context_->GetToastLocation());
}

void TryChromeDialog::CompleteInteraction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  endsession_observer_.reset();
  delegate_->SetExperimentState(state_);
  delegate_->InteractionComplete();
}

void TryChromeDialog::OnProcessNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  // Another browser process is trying to rendezvous with this one, which is
  // either waiting on FindTaskbarIcon to complete or waiting on the user to
  // interact with the dialog. In the former case, no attempt is made to stop
  // the search, as it is expected to complete "quickly". When it does complete
  // (in OnContextInitialized), processing will complete tout de suite. In the
  // latter case, the dialog is closed so that processing will continue in
  // OnWidgetDestroyed. OPEN_CHROME_DEFER conveys to this browser process that
  // it should ignore its own command line and instead handle that provided by
  // the other browser process.
  result_ = OPEN_CHROME_DEFER;
  state_ = installer::ExperimentMetrics::kOtherLaunch;
  if (popup_)
    popup_->Close();
}

void TryChromeDialog::OnWindowMessage(HWND window,
                                      UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  // wparam == FALSE means the system is not shutting down.
  if (message != WM_ENDSESSION || wparam == FALSE)
    return;

  // The ship is going down. Record the endsession event, but don't bother
  // trying to close the window or take any other steps to shut down -- the OS
  // will tear everything down soon enough. It's always possible that the
  // endsession is aborted, in which case the dialog may as well stay onscreen.
  result_ = NOT_NOW;
  state_ = installer::ExperimentMetrics::kUserLogOff;
  delegate_->SetExperimentState(state_);
}

void TryChromeDialog::GainedMouseHover() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  if (close_button_)
    close_button_->SetVisible(true);
}

void TryChromeDialog::LostMouseHover() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  if (close_button_)
    close_button_->SetVisible(false);
}

void TryChromeDialog::ButtonPressed(installer::ExperimentMetrics::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(result_, NOT_NOW);

  // Ignore this press if another press or a rendezvous has already been
  // registered.
  if (state_ != installer::ExperimentMetrics::kOtherClose)
    return;

  state_ = state;
  if (state_ == installer::ExperimentMetrics::kSelectedOpenChromeAndNoCrash)
    result_ = kExperiments[group_].result;

  popup_->Close();
}

void TryChromeDialog::OnWidgetCreated(views::Widget* widget) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(widget, popup_);

  popup_->GetNativeWindow()->AddPreTargetHandler(this);
}

void TryChromeDialog::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(widget, popup_);

  popup_->GetNativeWindow()->RemovePreTargetHandler(this);
  popup_->RemoveObserver(this);
  popup_ = nullptr;
  close_button_ = nullptr;

  CompleteInteraction();
}

void TryChromeDialog::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(popup_);

  switch (event->type()) {
    // A MOUSE_ENTERED event is received if the mouse is over the dialog when it
    // opens.
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_MOVED:
      if (!has_hover_) {
        has_hover_ = true;
        GainedMouseHover();
      }
      break;
    case ui::ET_MOUSE_EXITED:
      if (has_hover_) {
        has_hover_ = false;
        LostMouseHover();
      }
      break;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      if (has_hover_ && !display::Screen::GetScreen()->IsWindowUnderCursor(
                            popup_->GetNativeWindow())) {
        has_hover_ = false;
        LostMouseHover();
      }
      break;
    default:
      break;
  }
}
