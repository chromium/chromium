This directory holds the core logic that Chrome uses to launch external intents
on Android. External Navigation is surprisingly subtle, with implications on
privacy, security, and platform integration. This document goes through the
following aspects:
- Motivation
- Examples of supported and unsupported navigations
- What happens when a navigation is blocked
- Code Structure
- Additional Details
- Embedding of the component
- Differences between Chrome and WebLayer intent launching
- Opportunities for code health improvements in the component

Throughout this document we will be using Chrome's exercising of this component
for illustration.

# Motivation

The goal of External Navigation in Chrome is to seamlessly and securely
integrate the web with the Android app ecosystem. If the user has installed an
app for a website, then clicking a link to that website should cause the app to
open.

# Supported Flows

There are many ways web and app developers use intents, here are a few examples:

- Clicking a link to [https://www.youtube.com](https://www.youtube.com/) opens
 the Youtube app.
- Clicking a link to
[intent://www.youtube.com#Intent;scheme=https;action=android.intent.action.VIEW;S.browser\_fallback\_url=https%3A%2F%2Fwww.example.com/;end](http://www.youtube.com/#Intent;scheme=https;action=android.intent.action.VIEW;S.browser_fallback_url=https%3A%2F%2Fwww.example.com/;end)
 opens the Youtube app if installed, and example.com if not.
- An app opens a URL redirector in Chrome, which does a server redirect to an
 app link without any user interaction.
- Clicking a link triggers an async XHR to perform sign-in, then client
 redirects to an app after receiving the response.
- An app opens a CCT to start a payment, the user manually switches to their
 bank app to validate the payment, then later returns to the CCT, which, without
 user input, redirects back to the app after the payment is confirmed.

# Unsupported Flows

## In order to protect users, there are many things we don't allow:

- Launching an app without a
[Verified App Link](https://developer.android.com/training/app-links/verify-android-applinks)
 (or specialized intent filter pre-S) if the scheme can instead be handled in
 Chrome.
- Explicit intents (i.e. with a component specified), or intent without the
 BROWSABLE category (which Chrome always adds to intents).
- Launching an app from a tab not currently visible to the user.
- Launching an app without
[user activation](https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation)
 (unless doing trusted CCT/TWA navigation).
- Launching an app after a long timeout (as the user won't be expecting it).
- Launching certain schemes like file:, content: and other chrome-internal
 schemes.
- Launching Instant Apps directly on old Android versions.
- Launching apps from tab restore, back/forward, reload.
- Launching apps from fallback URLs or repeatedly attempting to launch apps with
 the same user activation (prevents fingerprinting).
- Launching arbitrary URLs in other browsers that may be behind on security
 patches.

## Other reasons to keep navigation in Chrome:

- Navigations from Chrome UI initially load in Chrome, but are allowed to
 redirect to apps.
- Form submissions are expected to be sent to a server, not an app, unless
 redirected.
- Intents URLs load in Chrome as they were intentionally sent to Chrome,
 though may redirect out.
- If an intent redirects to a URL for an Activity that the initial intent also
 matched (i.e. the set of apps supporting the navigation hasn't changed), we
 stay in Chrome as the user has already presumably chosen Chrome over the app.
- If an intent explicitly targeted the Chrome package, we interpret that as a
 strong signal the intent was supposed to stay in Chrome even through redirects.
- If the user is in incognito, keep them in incognito unless Chrome can't handle
 the URL, in which case we ask the user if they would like to leave.
- Same-host navigations shouldn't leave Chrome unless the list of handling apps
 changes (eg. going from google.com/search to google.com/maps should launch the
 Maps app, but going to google.com/search?page=2 should stay in Chrome).
- If in CCT or other contexts where the disambiguation dialog would be shown,
 but Chrome doesn't have an entry to put on it, but can handle the URL, the URL
 stays in Chrome.

## Debugging blocked Navigations

When trying to debug why a navigation was blocked, it's helpful to turn on
"External Navigation Debug Logs" in chrome://flags, which will cause detailed
logging to be output to logcat under the "UrlHandler" tag.

# What happens when a navigation is blocked

What happens when a navigation is blocked depends on the reason for the
navigation being blocked. Some URLs should never be launched to apps under any
circumstances, like content: schemes. In cases like this, the navigation will
simply be ignored, and a warning will be printed to the developer console. In
other cases, like when the external navigation was disallowed because Chrome
thinks the user probably expected to stay in Chrome (like navigation from typing
into the URL bar), it depends on whether Chrome can handle the link or not. If
Chrome can handle the link itself, or a fallback URL for an intent: URI exists,
it simply loads in Chrome. If Chrome can't handle the link, a Message is
displayed to the user asking them if they would like to leave Chrome.

# Code structure

The vast majority of the logic controlling whether a navigation leaves Chrome
lives in **ExternalNavigationHandler#shouldOverrideUrlLoading**. This function
makes use of the **ExternalNavigationDelegate** to allow content embedders to
customize the behavior, and the **RedirectHandler** to track Navigation history.

There are 2 ways shouldOverrideUrlLoading gets invoked:

- Main frame navigations
  - Called through a NavigationThrottle in intercept\_navigation\_delegate.cc
- Subframe navigations
  - Only intercepted for external protocols
  - Called through ExternalProtocolHandler::LaunchUrl

# Additional Details

## InterceptNavigationDelegateImpl

The entrypoint to the component is usually InterceptNavigationDelegateImpl.java,
which layers on top of //components/navigation_interception's support for
NavigationThrottles that delegate to Java for their core logic.  Within the
context of Chrome, InterceptNavigationDelegateImpl is a per-Tab (or Tab-like,
e.g. OverlayPanel) object that intercepts every main frame navigation made in
the given Tab and determines whether the navigation should result in an external
intent being launched. The key method is
InterceptNavigationDelegateImpl#shouldIgnoreNavigation(). This method sets up
state related to the current navigation and then invokes
ExternalNavigationHandler to do the heavy lifting of determining whether the
navigation should result in an external intent being launched. If so,
InterceptNavigationDelegateImpl does cleanup in the given Tab, including
restoring the navigation state to what it was before the navigation chain that
resulted in this intent being launched and potentially closing the Tab itself if
opening the Tab led to the intent launch.

## ExternalNavigationHandler

ExternalNavigationHandler is the core of the component. It handles all of the
intent launching semantics that Chrome has accumulated over 10+ years. See the
list of supported and unsupported flows above - this class is responsible for
the vast majority of that logic.

ExternalNavigationHandler.java is a large and complex class. The key external
entrypoint is ExternalNavigationHandler#shouldOverrideUrlLoading(), and the
method that actually holds the core logic for when and how external intents
should be launched is
ExternalNavigationHandler#shouldOverrideUrlLoadingInternal().

## RedirectHandler

This class tracks state across navigations in order to aid
InterceptNavigationDelegateImpl and ExternalNavigationHandler both in making
decisions on whether to launch an intent for a given navigation and in properly
handling the state within a Tab in the event that an intent is launched. Most
notably, it provides information about the redirect chain (if any) that a given
navigation is part of and whether the set of apps that can handle an intent
changes while processing the redirect chain. ExternalNavigationHandlerImpl uses
this information as part of its determination process (e.g., for determining
whether an intent can be launched from a user-typed navigation).
InterceptNavigationDelegateImpl also uses this information to determine how to
restore the navigation state in its Tab after an intent being launched.

## ExternalNavigationDelegate and InterceptNavigationDelegateClient

These interfaces allow embedders to customize the behavior of the component (see
the next section for details). Note that they should *not* be used to customize
the behavior of the components for tests; if that is necessary (e.g., to stub
out a production method), instead make the method protected and
@VisibleForTesting and override it as suitable in a test subclass.

## Other useful information

- Resource requests like XHR are not typically visible to the browser process,
 but sites often perform a client redirect to an app upon their completion. To
 support this we have a separate IPC coming from the renderer to update the
 RedirectHandler, see InterceptNavigationDelegate::OnResourceRequestWithGesture.
- Client-side redirects are challenging to deal with in general, as they're not
 considered part of the previous navigation, not directly associated with any
 user gesture, and generally poorly defined. The RedirectHandler deals with this
 by treating all renderer-initiated navigations without a user gesture as a
 client redirect on the previous navigation. This can lead to indefinite
 redirect chains, so in order to make sure an unattended page isn't redirecting,
 and the user isn't caught off guard, a hard 15 second timeout from the
 navigation with a user gesture is used.

# Embedding the Component

To embed the component, it's necessary to install
InterceptNavigationDelegateImpl for each "tab" of the embedder (where a tab is
the embedder-level object that holds a WebContents).

There are two interfaces that the embedder must implement in order to embed the
component: InterceptNavigationDelegateClient and ExternalNavigationDelegate.


# Differences between Chrome and WebLayer Embedding

In this section we highlight differences between the Chrome and WebLayer
embeddings of this component, all of which are encapsulated in the
implementations of the above-mentioned interfaces:

- Chrome has significant logic for handling interactions with handling of
  incoming intents (i.e., Chrome itself having been started via an intent).
  WebLayer's production use cases do not support being launched via intents,
  which simplifies WebLayer's implementations of
  InterceptNavigationDelegateClient and ExternalNavigationDelegate.
- WebLayer does not implement all of the integrations that Chrome does,
  notably with Android Instant Apps and Google Authenticator.
- WebLayer and Chrome Custom Tabs both have the notion of an
  "externally-initiated navigation": WebLayer via its public API surface, and
  CCT via an intent. However, as these mechanisms are different, the two
  embedders have different means of achieving similar semantics for these
  navigations: https://bugs.chromium.org/p/chromium/issues/detail?id=1087434.
- Chrome and WebLayer have different mechanisms for getting the last user
  interaction time, as documented here:
  https://source.chromium.org/chromium/chromium/src/+/main:weblayer/browser/java/org/chromium/weblayer_private/InterceptNavigationDelegateClientImpl.java;l=71?q=InterceptNavigationDelegateClientImpl&ss=chromium&originalUrl=https:%2F%2Fcs.chromium.org%2F

There are almost certainly further smaller differences, but those are the major
highlights.

# Opportunities for Code Health Improvements
- For historical reasons, there is overlap between the
  InterceptNavigationDelegateClient and ExternalNavigationDelegate interfaces.
  It is likely that the collective API surface could be thinned.
- It is also potentially even possible that the two interfaces could ultimately
  be merged into one.
