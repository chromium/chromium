// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/drag_and_drop_test_utils.h"

#import <AppKit/AppKit.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#import "ui/base/dragdrop/os_exchange_data_provider_mac.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

// Note on relationship to `ui/base/test/ui_controls_mac.mm`:
// `ui_controls` provides a process-global, synthetic-mouse-driven fake drag
// loop for `interactive_ui_tests` (and is disallowed in `browser_tests`).
// `DragAndDropSimulator` and `DragStartWaiter` provide deterministic,
// `WebContents`-targeted drag interception and injection for `browser_tests`
// without moving the global mouse cursor or relying on window z-order.

@interface FakeDraggingInfo : NSObject <NSDraggingInfo>
- (instancetype)initWithPasteboard:(NSPasteboard*)pasteboard
                  locationInWindow:(NSPoint)location
                 destinationWindow:(NSWindow*)window;
@end

@implementation FakeDraggingInfo
@synthesize draggingDestinationWindow = _draggingDestinationWindow;
@synthesize draggingSourceOperationMask = _draggingSourceOperationMask;
@synthesize draggingLocation = _draggingLocation;
@synthesize draggedImageLocation = _draggedImageLocation;
@synthesize draggedImage = _draggedImage;
@synthesize draggingPasteboard = _draggingPasteboard;
@synthesize draggingSource = _draggingSource;
@synthesize draggingSequenceNumber = _draggingSequenceNumber;
@synthesize animatesToDestination = _animatesToDestination;
@synthesize numberOfValidItemsForDrop = _numberOfValidItemsForDrop;
@synthesize draggingFormation = _draggingFormation;
@synthesize springLoadingHighlight = _springLoadingHighlight;

- (instancetype)initWithPasteboard:(NSPasteboard*)pasteboard
                  locationInWindow:(NSPoint)location
                 destinationWindow:(NSWindow*)window {
  if ((self = [super init])) {
    _draggingPasteboard = pasteboard;
    _draggingLocation = location;
    _draggedImageLocation = location;
    _draggingSourceOperationMask =
        NSDragOperationCopy | NSDragOperationLink | NSDragOperationMove;
    _draggingDestinationWindow = window;
    _numberOfValidItemsForDrop = 1;
    _draggingFormation = NSDraggingFormationNone;
    _springLoadingHighlight = NSSpringLoadingHighlightNone;
    _animatesToDestination = NO;
  }
  return self;
}

- (void)slideDraggedImageTo:(NSPoint)screenPoint {
}

- (NSArray<NSString*>*)namesOfPromisedFilesDroppedAtDestination:
    (NSURL*)dropDestination {
  return nil;
}

- (void)
    enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)enumOpts
                              forView:(NSView*)view
                              classes:(NSArray<Class>*)classArray
                        searchOptions:(NSDictionary*)searchOptions
                           usingBlock:(void (^)(NSDraggingItem* draggingItem,
                                                NSInteger idx,
                                                BOOL* stop))block {
}

- (void)resetSpringLoading {
}

@end

#pragma mark - DragAndDropSimulator

namespace drag_and_drop_test_utils {

namespace {

// Waits until `condition` evaluates to true using a nestable `RunLoop`.
// `base::test::RunUntil` uses `base::RunLoop::Type::kDefault`, which blocks
// application tasks when `SimulateDragEnter` or `SimulateDrop` is called
// inside `DragStartWaiter`'s nested run loop.
bool RunUntilNestable(base::FunctionRef<bool()> condition) {
  if (condition()) {
    return true;
  }
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(1),
              base::BindLambdaForTesting([&]() {
                if (condition()) {
                  run_loop.Quit();
                }
              }));
  run_loop.Run();
  return condition();
}

id<NSDraggingDestination> GetDraggingDestination(NSView* native_view) {
  if (!native_view || ![native_view window] ||
      [[native_view registeredDraggedTypes] count] == 0) {
    return nil;
  }
  return (id<NSDraggingDestination>)native_view;
}

}  // namespace

struct DragAndDropSimulator::PlatformState {
  PlatformState(content::WebContents* drag_contents,
                content::WebContents* drop_contents)
      : drag_contents_(drag_contents), drop_contents_(drop_contents) {}

  raw_ptr<content::WebContents> drag_contents_ = nullptr;
  raw_ptr<content::WebContents> drop_contents_ = nullptr;
  std::unique_ptr<ui::OSExchangeData> active_drag_data_;

  NSPasteboard* GetActivePasteboard() const {
    if (!active_drag_data_) {
      return nil;
    }
    return static_cast<const ui::OSExchangeDataProviderMac*>(
               &active_drag_data_->provider())
        ->GetPasteboard();
  }

  FakeDraggingInfo* CreateDraggingInfo(
      content::WebContents* target_contents,
      const gfx::Point& location,
      id<NSDraggingDestination> __strong* out_target) {
    NSPasteboard* pasteboard = GetActivePasteboard();
    if (!target_contents || !pasteboard) {
      return nil;
    }
    content::WebContents* root_contents =
        target_contents->GetOutermostWebContents();
    NSView* native_view = root_contents->GetNativeView().GetNativeNSView();
    id<NSDraggingDestination> target = GetDraggingDestination(native_view);
    if (!target) {
      return nil;
    }

    // `location` is in `WebContents` top-left coordinates (`gfx::Point`), while
    // `WebContentsViewCocoa` (`BaseView`) uses AppKit's unflipped bottom-left
    // coordinate system (`isFlipped == NO`) and flips `draggingLocation` back
    // to top-left via `GetSanitizedFlippedPoint(viewPoint, height)`.
    NSPoint view_point = NSMakePoint(
        location.x(), NSHeight([native_view bounds]) - location.y());
    NSPoint window_point = [native_view convertPoint:view_point toView:nil];

    *out_target = target;
    return [[FakeDraggingInfo alloc] initWithPasteboard:pasteboard
                                       locationInWindow:window_point
                                      destinationWindow:[native_view window]];
  }

  bool SendDragEnter(content::WebContents* target_contents,
                     const gfx::Point& location) {
    id<NSDraggingDestination> target = nil;
    FakeDraggingInfo* info =
        CreateDraggingInfo(target_contents, location, &target);
    if (!info) {
      return false;
    }
    content::WebContents* root_contents =
        target_contents->GetOutermostWebContents();
    [target draggingEntered:info];
    // `WebDragDest` performs an asynchronous hit test in `draggingEntered:`.
    // Wait until either `_dropDataFiltered` is populated (when accepted) or
    // the drag enter is rejected (e.g. by `WebContentsDelegate::CanDragEnter`).
    if (!RunUntilNestable([&]() {
          return root_contents->GetDropData() != nullptr ||
                 content::IsDragCanceledForTesting(root_contents);
        })) {
      ADD_FAILURE() << "Timed out waiting for async drag enter hit test.";
      return false;
    }
    [target draggingUpdated:info];
    return true;
  }

  bool SendDrop(content::WebContents* target_contents,
                const gfx::Point& location) {
    id<NSDraggingDestination> target = nil;
    FakeDraggingInfo* info =
        CreateDraggingInfo(target_contents, location, &target);
    if (!info) {
      return false;
    }
    content::WebContents* root_contents =
        target_contents->GetOutermostWebContents();
    // If the drag was rejected during `SendDragEnter` (e.g.
    // `WebContentsDelegate::CanDragEnter` returned false, leaving
    // `GetDropData()` null), AppKit ends the drag with `draggingExited:`
    // instead of calling `performDragOperation:`. Calling
    // `performDragOperation:` directly would bypass `_canceled` in
    // `WebDragDest::dropHitTestDidCompleteForView:`.
    if (!root_contents->GetDropData()) {
      [target draggingExited:info];
      content::CleanupDragStateForTesting(root_contents);
      return true;
    }
    [target draggingUpdated:info];
    if ([target performDragOperation:info] != YES) {
      return false;
    }
    // `WebDragDest::performDragOperation:` also performs an asynchronous hit
    // test and clears `_dropDataFiltered` once `dropHitTestDidCompleteForView:`
    // finishes dispatching the drop.
    if (!RunUntilNestable(
            [&]() { return root_contents->GetDropData() == nullptr; })) {
      ADD_FAILURE() << "Timed out waiting for async drop hit test.";
      return false;
    }
    return true;
  }
};

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* web_contents)
    : DragAndDropSimulator(web_contents, web_contents) {}

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* drag_contents,
                                           content::WebContents* drop_contents)
    : state_(std::make_unique<PlatformState>(drag_contents, drop_contents)) {}

DragAndDropSimulator::~DragAndDropSimulator() = default;

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const std::string& text) {
  auto data = std::make_unique<ui::OSExchangeData>();
  data->SetString(base::UTF8ToUTF16(text));
  return SimulateDragEnter(location, std::move(data));
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const GURL& url) {
  auto data = std::make_unique<ui::OSExchangeData>();
  data->SetURL(url, u"");
  return SimulateDragEnter(location, std::move(data));
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const base::FilePath& file) {
  auto data = std::make_unique<ui::OSExchangeData>();
  data->SetFilename(file);
  return SimulateDragEnter(location, std::move(data));
}

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    const std::vector<ui::FileInfo>& file_infos) {
  auto data = std::make_unique<ui::OSExchangeData>();
  data->SetFilenames(file_infos);
  return SimulateDragEnter(location, std::move(data));
}

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    std::unique_ptr<ui::OSExchangeData> data) {
  if (!data) {
    return false;
  }
  if (state_->active_drag_data_) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drag_data_ = std::move(data);
  if (!state_->SendDragEnter(state_->drag_contents_, location)) {
    state_->active_drag_data_.reset();
    return false;
  }
  return true;
}

bool DragAndDropSimulator::SimulateDrop(const gfx::Point& location) {
  if (!state_->active_drag_data_) {
    ADD_FAILURE() << "Cannot drop a drag that hasn't started yet.";
    return false;
  }
  if (state_->drag_contents_ && state_->drop_contents_ &&
      state_->drag_contents_->GetOutermostWebContents() !=
          state_->drop_contents_->GetOutermostWebContents()) {
    if (!state_->SendDragEnter(state_->drop_contents_, location)) {
      state_->active_drag_data_.reset();
      return false;
    }
  }
  bool success = state_->SendDrop(state_->drop_contents_, location);
  state_->active_drag_data_.reset();
  return success;
}

}  // namespace drag_and_drop_test_utils

#pragma mark - DragStartWaiter

@interface NSView (DragStartWaiterDonor)
- (NSDraggingSession*)
    cr_dragStartWaiter_beginDraggingSessionWithItems:
        (NSArray<NSDraggingItem*>*)items
                                               event:(NSEvent*)event
                                              source:
                                                  (id<NSDraggingSource>)source;
@end

namespace drag_and_drop_test_utils {

namespace {

class MacDragStartInterceptor {
 public:
  virtual ~MacDragStartInterceptor() = default;
  virtual NSDraggingSession* HandleBeginDraggingSession(
      NSView* view,
      NSArray<NSDraggingItem*>* items,
      NSEvent* event,
      id<NSDraggingSource> source) = 0;
};

MacDragStartInterceptor* g_active_interceptor = nullptr;

}  // namespace

struct DragStartWaiter::PlatformState : public MacDragStartInterceptor {
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  base::RunLoop release_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  base::OnceClosure on_drag_started_callback_;
  std::unique_ptr<ui::OSExchangeData> captured_data_;
  bool suppress_passing_further_ = false;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> swizzler_;

  PlatformState(content::WebContents* contents, base::OnceClosure callback)
      : web_contents_(contents),
        on_drag_started_callback_(std::move(callback)) {
    CHECK(web_contents_);
    CHECK(!g_active_interceptor)
        << "Only one DragStartWaiter can be active at a time.";
    g_active_interceptor = this;

    swizzler_ = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
        [NSView class], @selector(beginDraggingSessionWithItems:event:source:),
        @selector(
            cr_dragStartWaiter_beginDraggingSessionWithItems:event:source:));
  }

  ~PlatformState() override {
    release_loop_.Quit();
    swizzler_.reset();
    CHECK_EQ(g_active_interceptor, this);
    g_active_interceptor = nullptr;
  }

  NSDraggingSession* HandleBeginDraggingSession(
      NSView* view,
      NSArray<NSDraggingItem*>* items,
      NSEvent* event,
      id<NSDraggingSource> source) override {
    NSView* native_view = web_contents_->GetNativeView().GetNativeNSView();
    if (view != native_view && ![view isDescendantOf:native_view]) {
      return [view cr_dragStartWaiter_beginDraggingSessionWithItems:items
                                                              event:event
                                                             source:source];
    }

    CHECK(suppress_passing_further_)
        << "DragStartWaiter on macOS requires SuppressPassingStartDragFurther "
           "because AppKit's modal NSDraggingSession cannot run in automated "
           "tests without physical mouse button state.";

    captured_data_ = std::make_unique<ui::OSExchangeData>();
    NSPasteboard* pasteboard =
        static_cast<const ui::OSExchangeDataProviderMac*>(
            &captured_data_->provider())
            ->GetPasteboard();
    [pasteboard clearContents];
    NSMutableArray<id<NSPasteboardWriting>>* writers =
        [NSMutableArray arrayWithCapacity:[items count]];
    for (NSDraggingItem* item in items) {
      if (item.item) {
        [writers addObject:item.item];
      }
    }
    if ([writers count] > 0) {
      [pasteboard writeObjects:writers];
    }

    if (on_drag_started_callback_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_drag_started_callback_));
    }
    run_loop_.Quit();

    NSDraggingSession* fake_session =
        (NSDraggingSession*)[[NSObject alloc] init];
    if ([source
            respondsToSelector:@selector(draggingSession:willBeginAtPoint:)]) {
      [source draggingSession:fake_session willBeginAtPoint:NSZeroPoint];
    }
    release_loop_.Run();
    if ([source
            respondsToSelector:@selector(
                                   draggingSession:endedAtPoint:operation:)]) {
      [source draggingSession:fake_session
                 endedAtPoint:NSZeroPoint
                    operation:NSDragOperationCopy];
    }
    return fake_session;
  }
};

}  // namespace drag_and_drop_test_utils

@implementation NSView (DragStartWaiterDonor)
- (NSDraggingSession*)
    cr_dragStartWaiter_beginDraggingSessionWithItems:
        (NSArray<NSDraggingItem*>*)items
                                               event:(NSEvent*)event
                                              source:
                                                  (id<NSDraggingSource>)source {
  if (drag_and_drop_test_utils::g_active_interceptor) {
    return drag_and_drop_test_utils::g_active_interceptor
        ->HandleBeginDraggingSession(self, items, event, source);
  }
  return [self cr_dragStartWaiter_beginDraggingSessionWithItems:items
                                                          event:event
                                                         source:source];
}
@end

namespace drag_and_drop_test_utils {

DragStartWaiter::DragStartWaiter(content::WebContents* web_contents)
    : DragStartWaiter(web_contents, base::DoNothing()) {}

DragStartWaiter::DragStartWaiter(content::WebContents* web_contents,
                                 base::OnceClosure on_drag_started_callback)
    : state_(std::make_unique<PlatformState>(
          web_contents,
          std::move(on_drag_started_callback))) {}

DragStartWaiter::~DragStartWaiter() = default;

void DragStartWaiter::WaitUntilDragStart() {
  state_->run_loop_.Run();
}

void DragStartWaiter::ReleaseDrag() {
  state_->release_loop_.Quit();
}

std::unique_ptr<ui::OSExchangeData> DragStartWaiter::TakeCapturedData() {
  CHECK(state_->suppress_passing_further_ || state_->captured_data_)
      << "Cannot extract captured data unless drag is suppressed or completed.";
  return std::move(state_->captured_data_);
}

void DragStartWaiter::SuppressPassingStartDragFurther() {
  state_->suppress_passing_further_ = true;
}

}  // namespace drag_and_drop_test_utils
