# Messages UI

Message UI is an alternative to InfoBar UI on Android. It provides a set of APIs
and a consistent, ephemeral and trustworthy UI with various lifecycles and
priorities.

[TOC]

## Overview

Each message should include at least 3 properties: title, primary icon and
primary button text.

Each message will automatically be displayed, hidden and dismissed according to
given scope (see details below) and can be dismissed automatically or manually.
By default, each message will be automatically dismissed after around 10s after
it is displayed. The timer will be reset if the text or icon on the UI is
changed or if the Message is re-shown. Also, users can dismiss the message by
swiping the UI upwards, leftwards and rightwards. The feature clients can also
manually dismiss the message through provided APIs if necessary.


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
2. MessageIdentifier in `tools/metrics/histograms/enums.xml` [[1](https://chromium-review.googlesource.com/c/chromium/src/+/3139695/4/tools/metrics/histograms/enums.xml#55511)]
3. MessageIdentifier in `tools/metrics/histograms/metadata/android/histograms.xml` [[1](https://chromium-review.googlesource.com/c/chromium/src/+/3139695/4/tools/metrics/histograms/metadata/android/histograms.xml#97)]
4. MessageIdentifier string in `components/messages/android/java/src/org/chromium/components/messages/MessagesMetrics.java` [[1](https://chromium-review.googlesource.com/c/chromium/src/+/3139695/4/components/messages/android/internal/java/src/org/chromium/components/messages/MessagesMetrics.java#102)]


## Build a message model

All available model properties are defined in [components/messages/android/…/MessageBannerProperties.java](https://source.chromium.org/chromium/chromium/src/+/main:components/messages/android/java/src/org/chromium/components/messages/MessageBannerProperties.java)

Only some of them are required:

1. TITLE: the main text of the message UI
2. ICON / ICON_RESOURCE_ID: the primary icon of the message UI, located at the
   start side of the UI.
3. PRIMARY_BUTTON_TEXT: the label of the primary button, located at the end of
   the message UI.

The rest are optional, but some of those are very commonly used:

1. DESCRIPTION: the description / subtitle of the message UI, usually used to
   help explain the purpose of the message UI.
2. ON_PRIMARY_ACTION: the callback function triggered when the user clicks on
   the primary button. Only called once. After this function is triggered, the
   message itself will be dismissed.
3. ON_SECONDARY_ACTION / SECONDARY_BUTTON_MENU_TEXT / SECONDARY_ICON_CONTENT_DESCRIPTION
   / SECONDARY_ICON / SECONDARY_ICON_RESOURCE_ID: these are used to set a
   secondary action / menu. If SECONDARY_BUTTON_MENU_TEXT is configured, clicking
   on the secondary button will trigger a single-menu-item popup menu.
   Clicking on the secondary icon does not guarantee that the message will be
   automatically dismissed. We recommend the feature code to manually dismiss
   the message when secondary action callback is triggered.
    1. Note: there are changes in-flight to allow multiple menu items.
       Documentation will be updated once that lands.
4. ON_DISMISSED: the callback function triggered when the message UI is dismissed.
   Dismiss means the message has been removed from the queue and will not be displayed again.

You can refer to
[this CL](https://chromium-review.googlesource.com/c/chromium/src/+/3039479/17/chrome/android/java/src/org/chromium/chrome/browser/survey/ChromeSurveyController.java#239)
as an example on the Java side and
[this CL](https://chromium-review.googlesource.com/c/chromium/src/+/3161257/5/chrome/browser/android/oom_intervention/near_oom_reduction_message_delegate.cc#35)
as an example on the Native side.

Some other, less commonly used properties are:

* ICON_TINT_COLOR: the default icon color is blue. Use this to update the icon
  color and set TINT_NONE to disable the icon tint color.
* DESCRIPTION_MAX_LINES: set max lines of the description. The default when this
  property isn't set is to show all the description texts, which may occupy too much screen space.


## Enqueue your message model

After the model is defined and ready to be displayed, it should be enqueued by
calling MessageDispatcher#enqueueMessage and providing the model, scope,
and priority.


### MessageDispatcher

MessageDispatcher is per-window object. In Java, use
[MessageDispatcherProvider#from](https://source.chromium.org/chromium/chromium/src/+/main:components/messages/android/java/src/org/chromium/components/messages/MessageDispatcherProvider.java;l=35)
to get a dispatcher available in the current window, which can be null if native
initialization is not finished yet. In C++, use
messages::MessageDispatcherBridge::Get() instead.


### Scope

Message scope can also be seen as the life cycle of a message UI. It pre-defines
when and where messages should be displayed and dismissed. There are 3 scopes in
total (components/messages/android/message_enums.h):

1. **Navigation**: messages of navigation will be displayed only on the web page
   for which they are enqueued. It will be hidden (not dismissed) when the user
   switches to another tab and displayed again when the user returns to the
   target tab. It will be dismissed when user navigates to another page, such as
   navigating to [https://chromium.org](https://chromium.org) from
   [https://google.com](https://google.com) or navigating to
   [https://google.com/about](https://google.com/about) from
   [https://google.com/settings](https://google.com/settings), and also
   dismissed when the tab where the page lives is closed. This should be used
   when the content and purpose of the Message is tightly related to a certain
   page. For example, password messages should be only displayed on the page
   where the user submits the password and dismissed if the user navigates to
   another page.
2. **Window**: messages of window scope will be displayed as long as the current
   window is alive (current window is dead usually when app is closed or user
   merges windows). It can be displayed on any tab and web page. So this scope
   should be used only when the content and purpose of the message is unrelated
   to the web page. For example, Sync Error message is related to the app rather
   than the page and works as an app-level notification. Use
   #EnqueueWindowScopedMessage to enqueue a window-scoped message.
3. **Web_Contents**: this one is rarely used and usually not recommended unless
   really necessary. The only difference between web_contents scope and navigation
   scope is that messages of web_contents scope do not dismiss when the user
   navigates to another page; i.e. it is only dismissed when its associated
   tab is closed.


### Priority

There are two types of priority: urgent (a.k.a high) and normal (a.k.a low).

Urgent messages will be displayed ASAP,  in spite of enqueued messages of normal
properties. Urgent should only be used when the message is so important that you
want users to take an action ASAP, such as a serious security risk found on the
page the user is visiting. Otherwise, use normal in most cases.


## Dismiss your message

As explained in [Overview](#Overview), by default, the message will be dismissed
in the following cases:

* Timeout
* Swiping gesture
* Primary action button is clicked
* The given scope is destroyed

In addition, messages can be dismissed by feature client code so that the
message won’t be displayed any more. Use MessageDispatcher#dismissMessage to
dismiss the message. The dismiss callback will still be triggered.


## Ownership in native

In native, MessageDispatcherBridge#EnqueueMessage will return a MessageWrapper
object. The feature client is responsible for managing it. We recommend
dismissing the message manually if the MessageWrapper object is still alive when
the owner of the MessageWrapper object is destroyed.


## Test

On the Java side,
[components/messages/android/test/…/messages/MessagesTestHelper.java](https://source.chromium.org/chromium/chromium/src/+/main:components/messages/android/test/java/src/org/chromium/components/messages/MessagesTestHelper.java)
is available to get all current enqueued messages and get the property model of a certain message.

In native, components/messages/android/mock_message_dispatcher_bridge.h is
available to test whether a message is enqueued and trigger some callbacks
manually. You can refer to
[this CL](https://chromium-review.googlesource.com/c/chromium/src/+/3161257/5/chrome/browser/android/oom_intervention/near_oom_reduction_message_delegate_unittest.cc)
as an example.


## Built-in Metrics

Some metrics have been pre-defined to help evaluate the effectiveness of a message.

**Android.Messages.Enqueued**

Records the message identifier each time a message is enqueued through
MessageDispatcher. This histogram can be used for getting a count of messages
broken down by message identifier.

**Android.Messages.Dismissed.{MessageIdentifier}**

Records the reason why this message is dismissed, such as primary action, timer,
gesture and so on.

**Android.Messages.TimeToAction.Dismiss.{MessageIdentifier}**

Records the time interval the message was displayed on the screen before the
user dismissed it with a gesture. The metric is NOT recorded when the user
presses primary or secondary button or when the message is auto-dismissed based
on timer.

**Android.Messages.TimeToAction.{MessageIdentifier}**

Records the time interval the message was displayed on the screen before the
user explicitly dismissed it. The metric is recorded when the user presses the
primary or secondary button, or dismisses the message with a gesture.
