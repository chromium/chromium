# Messages UI

Message UI is an alternative to InfoBar UI on Android. It provides a set of APIs
and a consistent, ephemeral and trustworthy UI with various lifecycles and
priorities.

[TOC]

## Overview

Each message typically includes a `MESSAGE_IDENTIFIER`, a `TITLE`, a primary
icon (`ICON` or `ICON_RESOURCE_ID`), and a primary widget (`PRIMARY_BUTTON_TEXT`
or `PRIMARY_WIDGET_APPEARANCE = PrimaryWidgetAppearance.PROGRESS_SPINNER`).

Each message will automatically be displayed, hidden and dismissed according to
its given scope (see details below) and can be dismissed automatically or
manually. Up to two messages can be stacked on screen at the same time
(`Position.FRONT` and `Position.BACK`).

By default, the front message is automatically dismissed after ~10s (~30s when
accessibility gestures are enabled, adjusted by
`AccessibilityState.getRecommendedTimeoutMillis`). The auto-dismiss timer is
reset when the user touches the banner, when `TITLE` is updated, or when the
message is re-shown, and is paused while the secondary popup menu is open or
while any view inside `MessageContainer` holds accessibility focus.

Users can dismiss the front message by swiping upwards, leftwards, or
rightwards, by clicking the pointer-hover close button, or via the container's
accessibility dismiss action. Feature clients can also manually dismiss a
message through the provided APIs. To prevent accidental activation, the front
message enforces a `500ms` tap-protection period (`TAP_PROTECTION_DURATION_MS`)
that blocks touch events, non-system key events, and button clicks immediately
after the banner animates in or whenever its content/action properties are
mutated while visible.


## Developing a new Message UI Feature

You need to do the following things to enable your message UI, all described in
detail below.

1. [Declare your message UI](#Declare-your-message-ui) by adding a Message Identifier.
2. [Build a message model](#Build-a-message-model).
3. [Enqueue your message model](#Enqueue-your-message-model).


## Declare your message UI

You need to create a `MessageIdentifier` that represents your Message UI, which
distinguishes it from other message UIs and enables some feature-specific metrics
to be recorded.

The MessageIdentifier is defined as an `enum` and a string, which should be
appended in following files (you can refer to
[this CL](https://chromium-review.googlesource.com/c/chromium/src/+/3139695) as an example):

1. `components/messages/android/message_enums.h` [[1](https://chromium-review.googlesource.com/c/chromium/src/+/3139695/4/components/messages/android/message_enums.h#90)]
2. MessageIdentifier in `tools/metrics/histograms/metadata/android/enums.xml` [[1](https://chromium-review.googlesource.com/c/chromium/src/+/3139695/4/tools/metrics/histograms/enums.xml#55511)]
3. MessageIdentifier in `tools/metrics/histograms/metadata/android/histograms.xml` [[1](https://chromium-review.googlesource.com/c/chromium/src/+/3139695/4/tools/metrics/histograms/metadata/android/histograms.xml#97)]
4. MessageIdentifier string in `components/messages/android/java/src/org/chromium/components/messages/MessagesMetrics.java` [[1](https://chromium-review.googlesource.com/c/chromium/src/+/3139695/4/components/messages/android/internal/java/src/org/chromium/components/messages/MessagesMetrics.java#102)]


## Build a message model

All available model properties are defined in [components/messages/android/…/MessageBannerProperties.java](https://source.chromium.org/chromium/chromium/src/+/main:components/messages/android/java/src/org/chromium/components/messages/MessageBannerProperties.java)
(and exposed to C++ callers via
[components/messages/android/message_wrapper.h](https://source.chromium.org/chromium/chromium/src/+/main:components/messages/android/message_wrapper.h)).

Core properties:

1. `MESSAGE_IDENTIFIER`: the `MessageIdentifier` enum value used for recording
   histograms and applying per-message Finch configurations.
2. `TITLE`: the main text of the message UI.
3. `ICON` / `ICON_RESOURCE_ID`: the primary icon of the message UI, located at
   the start side of the UI.
4. `PRIMARY_BUTTON_TEXT` (or `PRIMARY_WIDGET_APPEARANCE =
   PrimaryWidgetAppearance.PROGRESS_SPINNER`): the label of the primary button
   (or an indeterminate progress spinner) located at the end of the message UI.

Commonly used optional properties:

1. `DESCRIPTION`: the description / subtitle of the message UI, usually used to
   help explain the purpose of the message UI.
2. `ON_PRIMARY_ACTION`: the callback triggered when the user clicks the primary
   button. In Java, this is a `Supplier<Integer>` returning a
   `PrimaryActionClickBehavior` value: `DISMISS_IMMEDIATELY` to dismiss the
   message after the callback runs, or `DO_NOT_DISMISS` to keep the message
   visible (for example, while transitioning `PRIMARY_WIDGET_APPEARANCE` to
   `PROGRESS_SPINNER`). In C++ (`MessageWrapper::SetActionClick`), the message
   is always dismissed immediately after the primary action callback runs.
3. `ON_SECONDARY_ACTION` / `SECONDARY_BUTTON_MENU_TEXT` /
   `SECONDARY_ICON_CONTENT_DESCRIPTION` / `SECONDARY_ICON` /
   `SECONDARY_ICON_RESOURCE_ID`: used to configure a secondary action button
   (`ListMenuButton`). Clicking the secondary button supports three modes:
   * **Custom or multi-item menu**: set `SECONDARY_MENU_BUTTON_DELEGATE` (or use
     `MessageWrapper::AddSecondaryMenuItem` in C++), optionally sizing the popup
     with `SECONDARY_MENU_MAX_SIZE` (`SMALL` 180dp, `MEDIUM` 250dp, `LARGE`
     300dp).
   * **Single-item popup menu**: set `SECONDARY_BUTTON_MENU_TEXT` +
     `ON_SECONDARY_ACTION`.
   * **Direct action callback**: set `ON_SECONDARY_ACTION` without
     `SECONDARY_MENU_BUTTON_DELEGATE` or `SECONDARY_BUTTON_MENU_TEXT`.

   `MessageBannerView` automatically sets a default content description on the
   secondary button (`@string/message_more_options`, `"More options for %1$s"`)
   using `TITLE` or `TITLE_CONTENT_DESCRIPTION` unless overridden via
   `SECONDARY_ICON_CONTENT_DESCRIPTION`. Clicking the secondary button does not
   automatically dismiss the message; feature code should call
   `MessageDispatcher#dismissMessage` explicitly if dismissal is desired.
4. `ON_DISMISSED`: the callback triggered with a `DismissReason` when the
   message UI is dismissed. Dismiss means the message has been removed from the
   queue and will not be displayed again.
5. `ON_FULLY_VISIBLE`: the callback (`Callback<Boolean>`) triggered when the
   message enters (`true`) or leaves (`false`) the foreground (`Position.FRONT`)
   state.

You can refer to
[this CL](https://chromium-review.googlesource.com/c/chromium/src/+/3039479/17/chrome/android/java/src/org/chromium/chrome/browser/survey/ChromeSurveyController.java#239)
as an example on the Java side and
[this CL](https://chromium-review.googlesource.com/c/chromium/src/+/3161257/5/chrome/browser/android/oom_intervention/near_oom_reduction_message_delegate.cc#35)
as an example on the Native side.

Other optional properties include:

* `ICON_TINT_COLOR`: the default icon tint is `@macro/default_icon_color_accent1`.
  Use this to customize the tint color or set `TINT_NONE` (`Color.TRANSPARENT`)
  to display an un-tinted multi-color icon.
* `LARGE_ICON` and `ICON_ROUNDED_CORNER_RADIUS_PX`: `LARGE_ICON` resizes the
  leading icon slot from `24dp` height to `36dp x 36dp`;
  `ICON_ROUNDED_CORNER_RADIUS_PX` rounds bitmap icon corners.
* `DESCRIPTION_ICON` and `RESIZE_DESCRIPTION_ICON`: attaches a leading compound
  drawable (`18dp` by default) to the description text.
* `DESCRIPTION_MAX_LINES` and `PRIMARY_BUTTON_TEXT_MAX_LINES`: limits the
  description or primary button text to the specified number of lines with
  `TruncateAt.END` ellipsizing.
* `TITLE_CONTENT_DESCRIPTION`: overrides the spoken accessibility label for
  `TITLE`.
* `PRIMARY_WIDGET_APPEARANCE`: controls the trailing widget using values from
  the `PrimaryWidgetAppearance` enum (`BUTTON_IF_TEXT_IS_SET` or
  `PROGRESS_SPINNER`), supported in both Java and C++
  (`MessageWrapper::SetPrimaryWidgetAppearance`).
* `DISMISSAL_DURATION`: sets a custom auto-dismissal timeout (in milliseconds)
  instead of the default ~10 seconds. Per-message timeouts can also be
  configured via the Finch parameter
  `autodismiss_duration_ms_<MessageIdentifier>` under
  `ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE` (see
  `ChromeMessageAutodismissDurationProvider`).


## Enqueue your message model

After the model is defined and ready to be displayed, it should be enqueued by
calling `MessageDispatcher#enqueueMessage` (or `enqueueWindowScopedMessage`) and
providing the model, scope, and priority.


### MessageDispatcher

MessageDispatcher is a per-window object. In Java, use
[MessageDispatcherProvider#from](https://source.chromium.org/chromium/chromium/src/+/main:components/messages/android/java/src/org/chromium/components/messages/MessageDispatcherProvider.java;l=34)
to get a dispatcher available in the current window, which can be null if native
initialization is not finished yet. In C++, use
`messages::MessageDispatcherBridge::Get()` instead.


### Scope

Message scope can also be seen as the life cycle of a message UI. It pre-defines
when and where messages should be displayed and dismissed. There are 4 scopes in
total (`components/messages/android/message_enums.h`):

1. **Navigation**: messages of navigation will be displayed only on the web page
   for which they are enqueued. It will be hidden (not dismissed) when the user
   switches to another tab and displayed again when the user returns to the
   target tab. It will be dismissed when the user navigates to another page,
   such as navigating to [https://chromium.org](https://chromium.org) from
   [https://google.com](https://google.com) or navigating to
   [https://google.com/about](https://google.com/about) from
   [https://google.com/settings](https://google.com/settings) (same-origin
   reloads do not dismiss the message), and also dismissed when the tab where
   the page lives is closed. This should be used when the content and purpose of
   the Message is tightly related to a certain page. For example, password
   messages should be only displayed on the page where the user submits the
   password and dismissed if the user navigates to another page.
2. **Window**: messages of window scope will be displayed as long as the current
   window is alive (current window is dead usually when app is closed or user
   merges windows). It can be displayed on any tab and web page. So this scope
   should be used only when the content and purpose of the message is unrelated
   to the web page. For example, Sync Error message is related to the app rather
   than the page and works as an app-level notification. Use
   `#enqueueWindowScopedMessage` / `#EnqueueWindowScopedMessage` to enqueue a
   window-scoped message.
3. **Web_Contents**: this one is rarely used and usually not recommended unless
   really necessary. The only difference between web_contents scope and navigation
   scope is that messages of web_contents scope do not dismiss when the user
   navigates to another page; i.e. it is only dismissed when its associated
   tab is closed.
4. **Origin**: messages of origin scope will be displayed on the page for which
   they are enqueued. Like navigation scope, it will be hidden (not dismissed)
   when the user switches to another tab and displayed again when the user
   returns to the target tab. However, unlike navigation scope, it is NOT
   dismissed when navigating to another page within the same origin (e.g. from
   https://google.com/settings to https://google.com/about). It will be dismissed
   if the user navigates to a page with a different origin or if the tab is closed.


### Priority, Stacking & Queue Suspension

There are two types of priority: urgent (a.k.a. `highPriority = true` /
`MessagePriority::kUrgent`) and normal (`highPriority = false` /
`MessagePriority::kNormal`).

Urgent messages are ordered ahead of normal-priority messages in
`MessageQueueManager`. Urgent should only be used when the message is so
important that you want users to take an action ASAP, such as a serious security
risk found on the page the user is visiting. Otherwise, use normal in most
cases.

Up to two messages from active scopes can be displayed at once (`Position.FRONT`
and a peeking `Position.BACK` card). `ChromeMessageQueueMediator` automatically
suspends the message queue across all scopes whenever Chrome is outside
`LayoutType.BROWSING` (such as in the Tab Switcher / Hub), while any
`ModalDialog` or `BottomSheet` is open, while the Omnibox has focus (resuming
`1000ms` after unfocus), while the activity is paused, or while top browser
controls are not yet fully visible.


## Dismiss your message

As explained in [Overview](#Overview), by default, the message will be dismissed
in the following cases:

* Auto-dismiss timeout (`DismissReason.TIMER`)
* Swiping gesture (`DismissReason.GESTURE`) or custom accessibility dismiss
  action (`@string/chrome_dismiss` / `@string/message_dismiss_and_show_next`)
* Pointer-hover close button click (`DismissReason.CLOSE_BUTTON`, enabled by
  default on the front message via `kMessagesCloseButton`)
* Primary action button click (`DismissReason.PRIMARY_ACTION`, when
  `ON_PRIMARY_ACTION` returns `PrimaryActionClickBehavior.DISMISS_IMMEDIATELY`)
* The associated scope is destroyed (`DismissReason.SCOPE_DESTROYED`)

In addition, messages can be dismissed by feature client code so that the
message won't be displayed any more. Use `MessageDispatcher#dismissMessage` (or
`MessageDispatcherBridge::DismissMessage` in C++) to dismiss the message; the
`ON_DISMISSED` callback will still be triggered.


## Ownership in native

In native, the feature client constructs and owns the `messages::MessageWrapper`
instance and passes `MessageWrapper*` to
`MessageDispatcherBridge::EnqueueMessage` (or `EnqueueWindowScopedMessage`),
which returns a `bool` indicating whether the message was enqueued. We recommend
dismissing the message manually via `MessageDispatcherBridge::DismissMessage` if
the `MessageWrapper` object is still in the queue when its owner is destroyed.


## Test

On the Java side,
[components/messages/android/test/…/messages/MessagesTestHelper.java](https://source.chromium.org/chromium/chromium/src/+/main:components/messages/android/test/java/src/org/chromium/components/messages/MessagesTestHelper.java)
is available to get all current enqueued messages and get the property model of a certain message.

In native, `components/messages/android/mock_message_dispatcher_bridge.h` is
available to test whether a message is enqueued and trigger some callbacks
manually. You can refer to
[this CL](https://chromium-review.googlesource.com/c/chromium/src/+/3161257/5/chrome/browser/android/oom_intervention/near_oom_reduction_message_delegate_unittest.cc)
as an example.


## Built-in Metrics

Metrics recorded by `MessagesMetrics.java` to evaluate the effectiveness and
visibility of a message include:

**Android.Messages.Enqueued** (and **.Visible** / **.Hidden**)

Records the `MessageIdentifier` each time a message is enqueued through
`MessageDispatcher`, as well as whether it was immediately visible or hidden
behind another message.

**Android.Messages.FullyVisible** / **Android.Messages.DismissedWithoutFullyVisible**

Records the `MessageIdentifier` when a message reaches the foreground
(`Position.FRONT`) or when it is dismissed without ever becoming fully visible.

**Android.Messages.Dismissed.{MessageIdentifier}**

Records the `DismissReason` why a message was dismissed, such as primary action,
timer, gesture, close button, or scope destruction.

**Android.Messages.TimeToAction.{MessageIdentifier}**

Records the time interval the message was displayed on the screen before the
user explicitly dismissed it via primary action, secondary action, or swipe
gesture.

**Android.Messages.TimeToFullyShow.{MessageIdentifier}**

Records the time interval from when the message was enqueued until it first
became fully visible in the foreground.
