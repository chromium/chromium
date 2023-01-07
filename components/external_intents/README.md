This directory holds the core logic that Chrome uses to launch external intents
on Android. Intent launching is surprisingly subtle, with implications on
privacy, security, and platform integration. This document goes through the
following aspects:
- Basic functionality and execution flow
- Embedding of the component
- Differences between Chrome and WebLayer intent launching
- Opportunities for code health improvements in the component

# Basic functionality

Below we give the basic functionality, using Chrome's exercising of this
component for illustration.

## InterceptNavigationDelegateImpl

The entrypoint to the component is InterceptNavigationDelegateImpl.java, which
layers on top of //components/navigation_interception's support for
NavigationThrottles that delegate to Java for their core logic.  Within the
context of Chrome, InterceptNavigationDelegateImpl is a per-Tab (or Tab-like,
e.g. OverlayPanel) object that intercepts every navigation made in the given Tab
and determines whether the navigation should result in an external intent being
launched. The key method is
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
intent launching semantics that Chrome has accumulated over 10+ years. This
includes the following:

- Disallowing launching of external intents for security reasons, e.g. when the
  app is in the background or non-user-generated navigations within subframes
- Ensuring that user-typed navigations can result in intents being launched
  only after a server-side redirect
- Ensuring that navigations resulting from a link click can result in intents
  being launched only with an associated user gesture
- Avoiding launching intents on back/forward navigations
- Warning the user before an intent is launched when the user is in incognito
  mode
- Detection of schemes for which intents should never be launched (e.g.,
  Chrome-internal schemes)
- Navigating to fallback URLs within the browser as specified by the given URL
- Keep same-host navigations within the browser
- Directing the user to the Play Store to install a given app if not present on
  the device
- Integrating with platform and Google features such as SMS, WebAPKs, Android
  Instant Apps and Google Authenticator
- The actual launching of external intents.

These are just a few of the highlights, as ExternalNavigationHandler.java is a
large and complex class. The key external entrypoint is 
ExternalNavigationHandler#shouldOverrideUrlLoading(), and the method that
actually holds the core logic for when and how external intents should be
launched is ExternalNavigationHandler#shouldOverrideUrlLoadingInternal().

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

# Embedding the Component

To embed the component, it's necessary to install
InterceptNavigationDelegateImpl for each "tab" of the embedder (where a tab is
the embedder-level object that holds a WebContents). For an example of a
relatively straightforward embedding, look at //weblayer's creation of
InterceptNavigationDelegateImpl.

There are two interfaces that the embedder must implement in order to embed the
component: InterceptNavigationDelegateClient and ExternalNavigationDelegate.
Again, //weblayer's implementation of these interfaces provides a good starting
point to follow. //chrome's implementations are significantly more complex for
several reasons: handling of differences between Chrome browser and Chrome
Custom Tabs, integration with handling of incoming intents, an extended set
of use cases, ....

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
