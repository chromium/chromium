# Split View Security FAQ

[TOC]

## Purpose

This document outlines common questions and security considerations related to the desktop Split View feature for Chromium. This is primarily written for an audience of Chromium developers and security researchers.

It is analogous to the general [Chrome Security FAQ](https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md).

## FAQ

### Why is there a thicker outline around one of the views in a split tab?

The thicker outline is a security indicator used to denote which of the tabs in a split view is currently **active**. Because many browser-level features (the Omnibox, site permissions, navigation buttons, etc.) are tied to a single active WebContents, it is critical that users can unambiguously identify which view is receiving focus. This prevents spoofing attacks where a malicious site might attempt to trick a user into attributing a browser action or indicator to the other visible site.

### Which actions require the active view indicator (thicker outline) to be present?

The active view indicator must be present when security-critical actions are being performed because they require clear site attribution. These security-critical actions include:
*   The Omnibox dropdown appearing (indicating the user is about to enter data or navigate).
*   Permission requests (e.g., camera, microphone, geolocation).
*   Safety Tip bubbles (e.g., lookalike domain warnings).
*   Device chooser bubbles (e.g., USB, Bluetooth, WebHID).

### How are tab-modal dialogs (like JavaScript alerts) handled in split view?

Tab-modal dialogs, which typically use a dark "modal overlay" scrim, are rendered only over the triggering view. The dialog itself is centered over that specific view to maintain clear attribution. If the user switches focus to the other view in the split, the dialog and its overlay remain visible but the active view indicator switches to the other tab.

Note that for very narrow browser windows, it is possible for the dialog to partially render on top of the other tab in the split view. Best effort is made to position the dialog over as much of the triggering tab as possible.

### Why is the Print dialog centered on the whole browser instead of the active view?

The Print dialog is a rare exception to the per-view centering rule. Because the Print dialog is very large and its content (the print preview) clearly shows which page is being printed, centering it over the entire browser window was deemed more usable. It also mitigates responsiveness issues that occur when rendering a complex preview in a very narrow split view.

### Can inactive tabs in a split view perform sensitive actions?

To ensure clear site attribution, certain sensitive actions are restricted to the **active** tab:
*   **Permission Prompts**: Permission requests from an inactive tab are suppressed until the tab is made active. [Bug: [428484827](https://crbug.com/428484827)]
*   **File Pickers**: Only the active tab is permitted to open a system file picker. This ensures the user knows exactly which site is requesting access to their local files. [Bugs: [440523110](https://crbug.com/440523110), [444653104](https://crbug.com/444653104)]

### How does Split View affect the Page Visibility API?

For the purposes of the `PageVisibility` API, both tabs in an active split view are considered **visible**. This is consistent with how partially occluded tabs or windows are treated in Chromium. Both pages will receive `visibilitychange` events and report a `visibilityState` of `visible`.

### Does an inactive tab in a split view have "user attention"?

No. Per the [User Attention](https://html.spec.whatwg.org/#user-attention) definition, an element only has user attention when it is visible and can receive system focus. Since only the active view in a split can receive focus, the inactive view does not have user attention.

### How does Split View protect against URL spoofing in the mini toolbar?

The mini toolbar (which shows origin information for the visible views) includes several mitigations to prevent origin spoofing:
*   **Tooltips**: Hovering over the origin in the mini toolbar reveals the full URL.
*   **Security Indicators**: Icons for active camera/mic usage, sound, and screensharing are displayed for all visible views.
*   **Data URL Handling**: For potentially misleading URLs (like crafted `data:` URLs), Chromium elides these from the tail to reduce spoofing risks, while standard URLs are elided from the head to prioritize the registrable domain (e.g., `...example.com` instead of `subdomain.exam...`). [Bug: [439876416](https://crbug.com/439876416)]
*   **Non-standard Links**: Certain non-standard links that are prone to security issues are blocked from opening in split view to mitigate risks. [Bug: [441143201](https://crbug.com/441143201)]

### Are extensions allowed to interact with Split View?

Yes. Extensions are granted access to a `splitId` property for tabs, which behaves similarly to `groupId`. Extensions can use this ID to identify which tabs are split and move or close them as a unit. Within the Extensions API, Chromium ensures that operations on split tabs fail gracefully (e.g., by unsplitting) if they would result in an invalid browser state.

### Why do we not allow certain dialogs to overlap the "line of death"?

The "line of death" is the boundary between browser-controlled UI (like the Omnibox) and web-controlled content. In Split View, we also maintain a visual boundary between the two sites. To prevent cross-site influence, browser-mediated dialogs are generally anchored and restricted to the side of the split they belong to, ensuring they do not overlap the content of the other site.

### What recording/media indicators are shown in Split View?

High-priority security information, such as camera, microphone, and screensharing access, is always displayed for **all** visible web pages in the mini toolbar. This ensures that a user is aware of sensor access even if the page using the sensor is currently inactive.

### Is the ability to see two sites at once a security risk?

In general, no. Viewing two sites side-by-side is a core feature of Split View and is not considered a vulnerability in itself. Security boundaries (such as the Same-Origin Policy and process isolation) are still strictly enforced between the two WebContents. A security bug would involve a breakdown of these boundaries or a failure of the UI to correctly communicate the state of either site to the user.

### Why are some dialogs allowed to be centered on the browser instead of the view?

As mentioned with the Print dialog, some very large or system-mediated dialogs are centered on the browser for usability. This is not considered a security bug as long as the dialog's content provides clear context about which page or action it is associated with, and it does not allow one site to perform actions on behalf of another without user consent.

### How are uncommitted or reverted navigations handled in Split View?

The Split View UI (including the mini toolbar and hovercards) must accurately reflect the state of the tab. If a navigation is reverted (for example, via `document.write()` from another window), the UI is cleared to show `about:blank` or the previously committed origin. This prevents the UI from displaying a URL that no longer matches the loaded content. [Bug: [442860746](https://crbug.com/442860746)]

### What should I do if I find a security bug in Split View?

Please report any such bugs [here](https://bugs.chromium.org/p/chromium/issues/entry?template=Security+Bug). Note that this is the general security bug reporting entrypoint so make clear that the bug is with the Split View feature for easier triage.