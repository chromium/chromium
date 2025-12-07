# Subresource Filter
The subresource_filter component deals with code that tags and filters
subresource requests based on:

1. Some page-level activation signal (e.g. should sub-resource filtering take place on this page).
2. A ruleset used to match URLs for filtering.

The primary consumer of this component is Chrome's ad filter, which filters ads
on pages that violate the [Better Ads Standard](https://www.betterads.org/standards/).

Additionally, Chrome will filter pages Safe Browsing determines are used for
phishing (i.e. on pages after the user has proceeded through the security
interstitial).

## High Level Description
At a high level, the component uses a memory mapped file of filtering rules to
filter subresource requests in Blink, as well as child frame navigations in the
browser process.

For historical reasons (intention to support iOS), code is split into two
components, [core](/components/subresource_filter/core) and
[content](/components/subresource_filter/content). The core code is code that
we could share with a non-content client like iOS, while all the content code
depends on the Content API.

Most of the logic in core deals with reading, indexing, and matching URLs off a
ruleset.

Most of the logic in content deals with tracking navigations, communicating with
the renderer, and interacting with the //chrome client code.

## Detailed Architecture
In this section, '=>' represents strong ownership, and '~>' is a weak reference.

### [core](/components/subresource_filter/core)
#### [core/browser](/components/subresource_filter/core/browser)
Code in core/browser is responsible for writing and indexing filtering rules.
The class that does most of this work is the RulesetService.

`BrowserProcessImpl`=>`ContentRulesetService`=>`RulesetService`

The `RulesetService` is responsible for indexing filtering rules into a
[Flatbuffer](https://google.github.io/flatbuffers/) format, and writing them to
disk. These rules come from the `RulesetServiceDelegate` as an `UnindexedRuleset`
which will be downloaded via the [Component Updater](/components/component_updater/README.md).
It also performs other tasks like deleting obsolete rulesets.

The code in this component also maintains a global `ConfigurationList`, which
defines how the entire subresource_filter component behaves.

#### [core/common](/components/subresource_filter/core/common)
The code in core/common deals with logic involved in filtering subresources that
is used in both browser and renderer processes. The most important class is the
`DocumentSubresourceFilter` which contains logic to filter subresources in the
scope of a given document.

In the browser process ownership looks like:
`ContentSubresourceFilterThrottleManager`=>`AsyncDocumentSubresourceFilter`=>`DocumentSubresourceFilter`

In the renderer, ownership looks like:
`DocumentLoader`=>`SubresourceFilter`=>`WebDocumentSubresourceFilterImpl`=>`DocumentSubresourceFilter`

### [content](/components/subresource_filter/content)
#### [content/shared](/components/subresource_filter/content/shared/)
The code in content/shared is not specific to Safe Browsing, but still depends
on content/. This will allow other subresource filtering use cases to be built
on top of the shared code, such as the [Fingerprinting Protection component]
(/components/fingerprinting_protection_filter/).

#### [content/browser](/components/subresource_filter/content/browser)
The content/browser code generally orchestrates the whole component.

##### Safe Browsing Integration - Page-level activation
Filtering for a given page is (mostly) triggered via Safe Browsing. The core
class that encapsulates that logic is the
`SubresourceFilterSafeBrowsingActivationThrottle`.

`SubresourceFilterSafeBrowsingActivationThrottle`=>`SubresourceFilterSafeBrowsingClient`=>`SubresourceFilterSafeBrowsingClientRequest`
The Safe Browsing client owns multiple Safe Browsing requests, and lives on the
UI thread.

Currently, the `SubresourceFilterSafeBrowsingActivationThrottle` checks every
redirect URL speculatively, but makes an activation decision based on the last
URL.

##### Document-level activation
The ruleset has rules for allowlisting documents in specific ways. How a given
document is activated is codified in the `ActivationState` struct.

In order to notify a document in the renderer about how it should be activated,
we read from the ruleset in the browser process and send an IPC to the frame at
ReadyToCommitNavigationTime.

This logic is Handled by the `ActivationStateComputingNavigationThrottle`.
`ActivationStateComputingNavigationThrottle`=>`AsyncDocumentSubresourceFilter`
This ownership is passed to the `ContentSubresourceFilterThrottleManager` at
`ReadyToCommitNavigation` time.

##### Child frame filtering
This component also needs to filter child frames that match the ruleset. This is
done by the `ChildFrameNavigationFilteringThrottle`, which consults its parent
frame's `AsyncDocumentSubresourceFilter`.

The code uses "root frame" and "child frame" terminology distinguish from the
FrameTree-centric "main frame" and "subframe". Frame trees may be embedded so
that a single "tab" may have multiple "main frames". An embedded main frame
(fenced frame) is treated by the filter like a subframe; a main frame in a
fenced frame is thus a subresource filter "child frame".

##### Throttle management
The `ContentSubresourceFilterThrottleManager` is a `WebContentsObserver`, and manages both the
`ActivationStateComputingNavigationThrottle` and the
`ChildFrameNavigationFilteringThrottle`. It maintains a map of all the activated
frames in the frame tree, along with that frame's current
`AsyncDocumentSubresourceFilter`, taken from the
`ActivationStateComputingNavigationThrottle`.

`ContentSubresourceFilterThrottleManager`=>`AsyncDocumentSubresourceFilter`

##### Ruleset management
Ruleset management in the browser process is done with handles that live on the
UI thread and access the ruleset asynchronously on a thread that allows IO.

In order to avoid accessing a potentially corrupt mmapped flatbuffers file in
the browser process, the handles also provide verification.

`ContentRulesetService`=>`VerifiedRulesetDealer::Handle`=>`VerifiedRulesetDealer`
`ContentSubresourceFilterThrottleManager`=>`VerifiedRuleset::Handle`=>`VerifiedRuleset`

#### [content/renderer](/components/subresource_filter/content/renderer)
The code in content/renderer deals with using the ruleset to match  and filter
resource requests made in the render process.

The most important class in the renderer is the `SubresourceFilterAgent`,
the `RenderFrameObserver` that communicates with the
`ContentSubresourceFilterThrottleManager`.

`SubresourceFilterAgent`~>`WebDocumentSubresourceFilterImpl`
