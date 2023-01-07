# Screen Time

This directory contains the integration between Chromium and the macOS
ScreenTime system, which is a digital wellbeing tool allowing users to restrict
their own use of apps and websites by category. ScreenTime was introduced in
macOS 11, but we only support it for macOS 12.1+ due to an
[issue with input handling](https://crbug.com/1202440).

The ScreenTime system API is documented [on
apple.com](https://developer.apple.com/documentation/screentime?language=objc).
The most pertinent class is `STWebpageController`, which is an
`NSViewController` subclass. Clients of ScreenTime construct a single
`STWebpageController` per tab and splice its corresponding NSView into their
view tree in such a way that it covers the web contents. The NSView becomes
opaque when screen time for that tab or website has been used up.

The public interface to ScreenTime within Chromium is the
`screentime::TabHelper` class, which is a
[TabHelper](../../../../../docs/tab_helpers.md) that binds an
STWebpageController to a WebContents.

There is also a key private class, called `screentime::HistoryBridge`, which
connects a
[HistoryService](../../../../../components/history/core/browser/history_service.h)
to the ScreenTime history deletion controller. HistoryBridge is a profile-keyed
service, so one exists for each Profile.

## Testing

So that tests can avoid depending on the real ScreenTime system,
`STWebpageController` is wrapped by a C++ class called
`screentime::WebpageController`, which has a testing fake called
`screentime::FakeWebpageController`, and `STWebHistory` is wrapped by a C++
class called `screentime::HistoryDeleter`.
