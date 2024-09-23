# Help Bubbles

[Help bubbles](./common/help_bubble.h) are the core visual component of both
[IPH](./feature-promos.md) and [Tutorials](./tutorials.md).

A help bubble is tinted bubble (blue in the default theme) with the following
elements:
 - Text
 - Title (optional)
 - Icon (optional)
 - Close button (X)
 - Action buttons (typically between zero and two)
 - Alternative and/or assist message for screen readers

Most (but not all) help bubbles also have a visible arrow that points to the
_anchor element_ - the UI element the bubble refers to. For example, a promo for
a new button in the toolbar would have an arrow pointing to the button.

Help bubbles can be created via a
[HelpBubbleFactory](./common/help_bubble_factory.h) and a
[HelpBubbleParams](./common/help_bubble_params.h) object, but in nearly all
cases you should be using the IPH or Tutorial system (or even
[ShowPromoInPage](/chrome/browser/ui/user_education/show_promo_in_page.h))
to display help bubbles.

> It's bad form to create your own help bubbles directly unless you are 100%
sure what you are doing! If you are tempted, please contact
[Frizzle Team](mailto:frizzle-team@google.com) first and see if there's a more
idiomatic way to achieve your goal.

## Help Bubble Arrows

When creating an IPH or Tutorial experience, you will want to specify the arrow
for each bubble. This specifies where the little visible arrow that points to
the anchor goes on the help bubble, and by extension, how the help bubble itself
positions itself.

For example if the arrow is `kTopRight`, then the arrow will be placed on top of
the help bubble, towards the right side of the bubble (left in RTL). This places
the help bubble below the anchor element, with the bulk of the bubble falling to
the left. Help bubbles can auto-reflect if they would fall outside the browser
window or the screen, but it's best to choose an arrow that maximizes the
likelihood that the bubble will fit, and minimizes interference with UI the user
will be trying to interact with.

Your UX designer should have already determined the ideal arrow positioning in
their specifications; by default you should find the option that best matches
their design.

### "No Arrow"

The `HelpBubbleArrow::kNone` option places the help bubble directly beneath the
anchor, with no visible arrow. This is almost always used with the
`kTopContainerElementId` element, which results in the help bubble sitting just
below the toolbar (or bookmarks bar if present), in the middle of the browser
window. Be careful when using this option, because it is the most obtrusive
position for a help bubble possible!

## Anchor Elements

As mentioned above, any UI element may be an anchor. This is handled through the
[ElementTracker](/ui/base/interaction/element_tracker.h) system. This associates
a `TrackedElement` object with each UI element that is properly tagged with an
[ElementIdentifier](/ui/base/interaction/element_identifier.h) and visible to
the user.

It is sufficient to assign an identifier; the tracker will do the rest. However,
note that a help bubble cannot show if the anchor element is not visible, as
there may be no corresponding `TrackedElement`.

### Creating and Assigning Identifiers

Element identifiers are declared via macros such as
`DECLARE/DEFINE_ELEMENT_IDENTIFIER_VALUE()`. Convention is to put identifiers in
either
[`browser_element_identifiers.h`](/chrome/browser/ui/browser_element_identifiers.h)/[`.cc`](/chrome/browser/ui/browser_element_identifiers.cc)
or in the class which conceptually owns the identifier, using
`DECLARE/DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE()`.

For most `View`s, assign an identifier via
`SetProperty(kElementIdentifierKey, <element_id>)`. For menu items (both Views
and Mac native context menus), use `SimpleMenuModel::SetElementIdentifierAt()`.
For WebUI elements, the process is more complex; see
[this documentation](./webui/README.md) for info.

### Element Contexts

A concept you will run into when trying to anchor help bubbles is the
`ElementContext`. Each potential anchor element exists within a context, and
each help bubble will look for an anchor in a specific context or contexts.

Contexts exist to handle cases where there are multiple browser windows, so that
the user cannot trigger a help bubble in a different window by accident. When
showing an IPH or Tutorial help bubble, the default is to look for the anchor in
the current context only.

Contexts exist for:
 - A browser or app window and all menus and secondary UI associated with that
   window.
 - A WebUI page.
 - Some tab-modal dialogs.

> The reason for the latter two being their own contexts is because tabs can be
  moved between windows. This creates some complexity that will hopefully be
  remedied with future updates.

If you are trying to show a help bubble through either the IPH or Tutorials
system and the bubble is not appearing, consider whether the
`FeaturePromoSpecification` or tutorial step needs to explicitly specify "in any
context". This usually happens when attempting to show a help bubble anchored to
a WebUI page.

### Anchor Element Filters

In some cases, you may be able to apply a "filter" function to the anchor of a
help bubble. What this means is that the eligible anchor element(s) (based on
identifier and context) will be passed to a function that can do _pretty much
anything_ and then return the actual anchor element. The returned element need
not be one of the initial elements found, and can be any UI element.

Filters are often used for:
 - Selecting one out of a set of dynamic items, such as tabs, that all share the
   same identifier.
 - Locating a UI element that was not assigned an identifier due to technical
   limitations, such as specific rows of information in a cookies dialog.
    - In this case, the initial anchor ID will usually be for the dialog or
      container, and then the actual anchor is located from there.
    - It is common to use `ElementTrackerViews::GetElementForView(..., true)`
      in order to create a temporary TrackedElement for the actual anchor.
    - This approach does not work well for non-Views elements.

## Available Help Bubble Implementations

The help bubble infrastructure is presentation framework-agnostic. Currently,
the following help bubble types are supported in Chrome Desktop:
1. Views help bubble anchored to a View
2. Views help bubble anchored to a native Mac menu
3. Views help bubble anchored to a WebUI element in a non-tab WebUI
4. WebUI help bubble anchored to a WebUI element in a tab WebUI

The correct type of help bubble implementation is chosen automatically based on
the anchor view.

Some other platforms (e.g. ChromeOS) have created additional types of help
bubbles for specific applications; creating a new help bubble implementation
isn't particularly hard; see existing classes derived from
[HelpBubble](./common/help_bubble.h) and
[HelpBubbleFactory](./common/help_bubble_factory.h).

The key takeaway is that in the vast majority of User Education code, the logic
around help bubbles doesn't (and shouldn't!) care which factory or which help
bubble implementation is being used; all of the logic should be in the
factories.

If you are considering extending help bubbles to new frameworks, feel free to
reach out to [Frizzle Team](mailto:frizzle-team@google.com) for guidance.
