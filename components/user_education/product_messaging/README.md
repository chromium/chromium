# Product Messaging Controller

This is a profile-level controller which mediates spontaneous UI throughout
chromium. The goal is to prevent the user from being overwhelmed by spontaneous
messaging, and to prevent different spontaneous UI from fighting with each other.

For example, the *Product Messaging Controller* guarantees that mandatory
compliance notices do not have IPH popping up over them.

Current/likely candidates for Product Messaging Controller:
 - IPH (spontaneous blue help bubbles).
 - Mandatory Legal/Compliance notices, privacy messaging, etc.
 - Page-contextual cues.
 - Website "log in with Google" prompts.
 - etc.

Things that should not interact with the PMC:
 - User-initiated dialogs and pop-ups.
 - Passive or background elements such as NTP promos and "New" Badges.

## Overview

The *Product Messaging Controller* is a multi-lane queue that allows spontaneous
messages to ask to be shown, and be granted permission to show when it is safe
to do so. Once granted permission, the requesting system holds a handle for as
long as the UI is showing, then discards it; discarding the handle allows the
next waiting message to show.

When a lower-priority message is showing and a higher-priority message wants to
show, the lower-priority system can ask to be notified, and will be told if/when
it is *superseded*. The lower-priority system can then choose to hide or modify
its UI to accommodate the higher-priority message. (It can also choose not to do
anything in response.)

## Onboarding a New System to Product Messaging Controller

This is for new message types not covered by the `ProductMessageType` enum in
[product_messaging_types.h](./product_messaging_types.h). If you're adding a
message of an existing type, skip to the next section entitled "Creating and
Showing Your Message".

You should consult with one of the [OWNERS](./OWNERS) before onboarding a new
system.

1. Add your messaging type(s) to the `ProductMessageType` enum.
   - Typically you will want a value equivalent to or less than
     _kLowPriorityIph_.
   - This is because you might want an IPH to display on your UI.
2. Add any specific configuration required by your messages type(s) to
   `ProductMessagingPolicyImpl::CreateDefault()`:
   - Use `SetSelfBlocking(false)` if you want your messages to be able to
     re-show in the same session, or if you are reusing the same key for
     different messages.
   - Use `SetEquivalent()` to specify that the two types should share a queue
     and priority; they will then block but not supersede each other.
3. Add any message-specific configuration:
   - `SetBlockedBy()` indicates that the given message cannot be shown if any of
     the specified messages are queued, showing, or have been shown.
   - `SetShowAfter()` indicates that the given message always goes behind the
     other specified messages in the queue.
   - Note that both of these are rarely used.

## Creating and Showing Your Message

To prepare to show your message, determine if you need a new key. Some systems
use a single key for all messages (this is usually combined with
`SetSelfBlocking(false)` - see above), while others have a separate key for each
message.

Use the `DECLARE/DEFINE_...` macros in
[product_messaging_types.h](./product_messaging_types.h) to create a new message
key with a specified priority.

### Requesting to Show

When you are ready to show, retrieve the `ProductMessagingController` from the
`UserEducationService` for the current profile and call `QueueMessage()`.

You will specify a `ProductMessageReadyCallback` that receives a
`ProductMessagingHandle` if/when your message is ready to show. If it is not (or
if you specify a timeout and it elapses), it will be ejected from the queue.

```cpp
  BrowserUserEducationService* const service =
      UserEducationServiceFactory::GetForBrowserContext(profile);
  service->product_messaging_controller()->QueueMessage(
      kMyMessageKey,
      base::BindOnce(&MySystem::OnMessageReady, GetAsWeakPtr()));
```

### Showing Your Message

When your `ProductMessageReadyCallback` is called, you should:
 - Determine if you still/really want to show your message.
   - If not, discard the handle immediately.
 - Call `ProductMessagingHandle::SetShown()` to indicate your message was shown.
 - Show your UI.
   - If possible store the handle in your UI so that when the UI is torn down
     the handle is disposed.
   - If not, wait for your UI to be dismissed and discard the handle to indicate
     you are done.

Here's an example of what the code might look like:

```cpp
  void MySystem::OnMessageReady(ProductMessagingHandle handle) {
    if (ShouldShowSpontaneousUi()) {
      handle->SetShown();
      ShowSpontaneousUi(std::move(handle));
    }
  }
```

### Listening for Superseded Events

If your UI is lower-priority you may want to respond to higher-priority UI being
shown. When you get the callback to show your UI and you decide to show it,
first call `ProductMessagingHandle::SetSupersededCallback()`.

You will now get callbacks if a higher priority message is shown. You can choose
what to do with your UI at this point - you might want to hide or reduce its
prominence in some cases but not in others.

```cpp
  void MySystem::OnMessageReady(ProductMessagingHandle handle) {
    if (ShouldShowSpontaneousUi()) {
      handle->SetShown();
      handle->SetSupersededCallback(base::BindOnce(
          &MySystem::OnSpontaneousUiSuperseded, GetAsWeakPtr()));
      ShowSpontaneousUi(std::move(handle));
    }
  }

  void MySystem::OnSpontaneousUiSuperseded(
      ProductMessageKey, ProductMessageStatus) {
    CloseSpontaneousUiIfShowing();
  }
```

## FAQ

*Q: If the queue is empty, how long will it take to get called back?*

A: Callbacks are never synchronous, but will typically happen on the next frame.

*Q: Is there a way to tell if my message would be able to show immediately?*

A: For now, call `ProductMessagingController::GetAllMessages()` with appropriate
filters (e.g. `priority_higher_than` equal to the next priority level strictly
lower than your message). If enough systems end up asking the same question we
can always add a convenience method.

*Q: How do I know if my message timed out or was blocked?*

A: Call `ProductMessagingController::GetMessageStatus()` and see if your message
is still queued. We don't [yet] provide a timeout/blocked callback because the
"re-queueing a message resets the timeout" behavior makes determining when to
fire such a callback much harder to evaluate.

*Q: I need more sophisticated rate-limiting/prioritization!*

A: This system exists only to coordinate different systems which want to display
spontaneous UI, not to coordinate rate-limiting or prioritization within a
single system.

If the change you need is strictly one for coordinating between different
systems, talk to the maintainers and the policies can potentially be made more
sophisticated.
