# User Education component library

This library contains the code that will allow you to implement
**In-Product-Help (IPH)** and **Tutorials** in any framework, as well as display
the **"New" Badge** on menus and labels.

## Upstream dependencies

Familiarity with these libraries are strongly recommended; feel free to reach
out to their respective OWNERS if you have any questions.

  * [UI Interaction](/ui/base/interaction/README.md)
    * [ElementTracker](/ui/base/interaction/element_tracker.h) - supplies anchor
      points for help bubbles
    * [InteractionSequence](/ui/base/interaction/interaction_sequence.h) -
      describes the situations in which a Tutorial advances to the next step
  * [Feature Engagement](/components/feature_engagement/README.md) - used to
    evaluate triggering conditions for IPH and New Badge.

## Directory structure

 * [common](./common) - contains platform- and framework-agnostic APIs for
   working with `HelpBubble`s, **IPH**, and **Tutorials**.
 * [test](./test) - contains common code for testing user education primitives
 * [views](./views) - contains code required to display a `HelpBubble` in a
   Views-based UI, as well as **"New" Badge** primitives.
 * [webui](./webui/README.md) - contains code required to display a `HelpBubble`
   on a WebUI surface.

# Programming API

## Help bubbles

The core presentation element for both IPH and Tutorials is the
[HelpBubble](./common/help_bubble.h). A `HelpBubble` is a blue bubble that
appears anchored to an element in your application's UI and which contains
information about that element. For example, a `HelpBubble` might appear
underneath the profile button the first time the user starts Chrome after
adding a second profile, showing the user how they can switch between profiles.

Different UI frameworks have different `HelpBubble` implementations; for
example, [HelpBubbleViews](./views/help_bubble_factory_views.h). Each type of
`HelpBubble` is created by a different
[HelpBubbleFactory](./common/help_bubble_factory.h), which is registered at
startup in the global
[HelpBubbleFactoryRegistry](./common/help_bubble_factory_registry.h). So for
example, Chrome registers separate factories for Views and WebUI, and on Mac
a third factory that can attach a Views-based `HelpBubble` to a Mac native menu.

To actually show the bubble, the `HelpBubbleFactoryRegistry` needs two things:
  * The `TrackedElement` the bubble will be anchored to
  * The [HelpBubbleParams](./common/help_bubble_params.h) describing the bubble

You will notice that this is an extremely bare-bones system. ***You are not
expected to call `HelpBubbleFactoryRegistry` directly!*** Rather, the IPH and
Tutorial systems use this API to show help bubbles.

## In-Product Help (IPH)

In-Product Help is the simpler of the two ways to display help bubbles, and can
even be the entry point for a Tutorial.

IPH are:
 * **Spontaneous** - they are shown to the user when a set of conditions are
   met; the user does not choose to see them.
 * **Rate-limited** - the user will only ever see a specific IPH a certain
   number of times, and will only see a certain total number of different IPH
   per session.
 * **Simple** - only a small number of templates approved by UX are available,
   for different kinds of User Education journeys.

Your application will provide a
[FeaturePromoController](./common/feature_promo_controller.h) with a
[FeaturePromoRegistry](./common/feature_promo_registry.h). In order to add a new
IPH, you will need to:
 1. Add the `base::Feature` corresponding to the IPH.
 2. Register the
    [FeaturePromoSpecification](./common/feature_promo_specification.h)
    describing your IPH journey (see below).
 3. Configure the Feature Engagement backend for your IPH journey
    ([see documentation](/components/feature_engagement/README.md)).
 4. Put hooks in your code:
    * Call `FeaturePromoController::MaybeShowPromo()` at the point in the code
      when the promo should trigger, adding feature-specific logic for when the
      promo is appropriate.
    * Add additional calls to `feature_engagement::Tracker::NotifyEvent()` for
      events that should affect whether the IPH should display.
      * These should also be referenced in the Feature Engagement configuration.
      * This should include the user actually engaging with the feature being
        promo'd.
      * You can retrieve the tracker via
        `FeaturePromoControllerCommon::feature_engagement_tracker()`.
    * Optionally: add calls to `FeaturePromoController::CloseBubble()` or
      `FeaturePromoController::CloseBubbleAndContinuePromo()` to
      programmatically end the promo when the user engages your feature.
 5. Enable the feature via a trade study or Finch.

### Registering your IPH

You will want to create a `FeaturePromoSpecification` and register it with
`FeaturePromoRegistry::RegisterFeature()`. There should be a common function
your application uses to register IPH journeys during startup; in Chrome it's
`MaybeRegisterChromeFeaturePromos()`.

There are several factory methods on FeaturePromoSpecification for different
types of IPH:
  * **CreateForToastPromo** - creates a small, short-lived promo with no buttons
    that disappears after a short time.
    * These are designed to point out a specific UI element; you will not expect
      the user to interact with the bubble.
    * Because of this a screen reader message and accelerator to access the
      relevant feature are required; this will be used to make sure that screen
      reader users can find the thing the bubble is talking about.
  * **CreateForSnoozePromo** - creates a promo with "got it" and "remind me
    later" buttons and if the user picks the latter, snoozes the promo so it can
    be displayed again later.
  * **CreateForTutorialPromo** - similar to `CreateForSnoozePromo()` except that
    the "got it" button is replaced by a "learn more" button that launches a
    Tutorial.
  * **CreateForLegacyPromo (DEPRECATED)** - creates a toast-like promo with no
    buttons, but which does not require accessible text and has no or a long
    timeout. *For backwards compatibility with older promos; do not use.*

You may also call the following methods to add additional features to a bubble:
  * **SetBubbleTitleText()** - adds an optional title to the bubble; this will
    be in a larger font above the body text.
  * **SetBubbleIcon()** - adds an optional icon to the bubble; this will be
    displayed to the left (right in RTL) of the title/body.
  * **SetBubbleArrow()** - sets the position of the arrow relative to the
    bubble; this in turn changes the bubble's default orientation relative to
    its anchor.

These are advanced features
  * **SetInAnyContext()** - allows the system to search for the anchor element
    in any context rather than only the window in which the IPH is triggered.
  * **SetAnchorElementFilter()** - allows the system to narrow down the anchor
    from a collection of candidates, if there is more than one element maching
    the anchor's `ElementIdentifier`.

## Tutorials

[TBD]

## New Badge

[TBD]

## Adding User Education to your application

There are a number of virtual methods that must be implemented before you can
use these User Education libraries in a new application, mostly centered around
localization, accelerators, and global input focus.

Fortunately for Chromium developers, the browser already has the necessary
support built in for Views, WebUI, and Mac-native context menus. You may refer
to
[browser_user_education_service](/chrome/browser/ui/views/user_education/browser_user_education_service.h)
for an example that could be extended to other (especially Views-based)
platforms such as ChromeOS.