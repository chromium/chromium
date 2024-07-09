# User Education Tutorials

Once you have the PRD spec for your tutorial, here are the steps you will follow
to create the promo:
1. Create your tutorial identifier and metrics.
2. Register and describe your tutorial.
3. Add an entry point for your tutorial.
4. Manually test (and optionally, a write regression test) for your tutorial.

## Create your Tutorial Identifier and Metrics

A `TutorialIdentifier` is a unique string. Create a new ID for your tutorial and
add it to
[`tutorial_identifiers.h`](/chrome/browser/user_education/tutorial_identifiers.h)
and [`.cc`](/chrome/browser/user_education/tutorial_identifiers.cc).

In the same file, you should also create your "metrics prefix", which will be
used for recording histograms. Unlike the tutorial ID, the histogram prefix
should be _CamelCase_ with no special characters or spaces.

Finally, add the metrics prefix you defined above to the "TutorialID" variants
block in
[`/tools/metrics/histograms/metadata/user_education/histograms.xml`](/tools/metrics/histograms/metadata/user_education/histograms.xml).
Be sure to use the histogram prefix and not the full tutorial name/ID.

## Register and describe your Tutorial

In
[`browser_user_education_service.cc`](/chrome/browser/ui/views/user_education/browser_user_education_service.cc),
in `MaybeRegisterChromeTutorials()`, you should create your
`TutorialDescription` and register it with the provided `TutorialRegistry`.

The basic pattern is as follows:

```
  // Create the tutorial with histograms and steps:
  auto my_tutorial_description =
      TutorialDescription::Create<kMyTutorialMetricPrefix>(
          // Tutorial steps go here...
      );
  
  // Add metadata. Note that this could be assigning an entire metadata object
  // or setting individual fields of the existing `metadata` member.
  // The minimum recommended fields are "additional_description", "milestone",
  // and "owners"; in the future these will become required.
  my_tutorial_description.metadata = ...

  // Actually register the tutorial:
  tutorial_registry.AddTutorial(
      kMyTutorialId,
      std::move(my_tutorial_description));
```

All of this should be fairly straightforward except the steps, and there are
plenty of examples in the existing code.

### Defining Tutorial steps

There are four general kinds of steps, all defined in `tutorial_descriptions.h`,
and all of which target a UI element by `ElementIdentifier`:
 - **BubbleStep** - shows a help bubble anchored to the specified element.
 - **EventStep** - waits for the code to fire a custom event via
   `ElementTracker` on the specified element.
 - **HiddenStep** - waits for the element to be shown or hidden, or the user to
   click on a button or menu item, without showing a bubble.
 - **If** (or the convenience class **IfView**) - checks to see if the element
   (or View) is present and fulfils some predicate you define; if it does, then
   the steps in `.Then(...)` are executed; else the steps in `.Else(...)`
   (optional) are executed.

Each step can be further modified by member functions on either the step class
or the base `Step` class. These are things like specifying what context to look
for the target element in, whether the element must be present at the start or
end of the step, etc.

See [Help Bubbles](./help-bubbles.md) for more information on UI elements,
identifiers, contexts, etc.

### When to use conditionals

If you have multiple variations on a Tutorial based on UI state or some easily-
checkable condition, then using `If` or `IfView` is preferable to creating
multiple versions of the Tutorial. This is especially true if the Tutorial is
repeatable and running through it once changes the state in such a way that the
other variation would need to be shown. (Example: Tab Groups tutorial created
a tab group, so some of the bubbles need to change to reference the fact that
there is already a tab group).

Note that the "progress bar" shown in each tutorial bubble is based on the
longest possible journey through the Tutorial, so if a conditional step skips
steps or shows an abridged version of the tutorial, the progress may jump from
e.g. 1/6 to 3/6.

### Steps: notes and suggestions

When creating Tutorials, the flow should be:
1. Show a bubble describing the next action the user should take
2. Observe some change that is the result  (`EventStep` or `HiddenStep`)
3. Show the next bubble with further instructions

If (3) involves anchoring the next bubble to an element that will appear as a
result of the action prompted in (1), you can omit (2). This is because there is
an implied "wait for the target element to become visible" in all `BubbleStep`s.

On the other hand, if (1) and (3) both reference UI elements that are already
visible when the Tutorial starts, the first bubble will show, immediately be
hidden, and then the second bubble will show; the user will never actually see
the first bubble. In this case (2) is mandatory and should watch for either the
user input or a resulting event or UI change.

The final Tutorial step must be a bubble step, and will mark the "success"
state. It may be given additional buttons or icons reflecting this state. This
bubble will persist until the user dismisses it, or it is forced to be hidden
for other reasons (such as the UI it is anchored to disappearing).

Just generally, think about how your UI works. Think about the things the user
might do and how the UI might react. For example, if there's a chance the user
is likely to dismiss an element you want to show a bubble on in a way that won't
allow the tutorial to continue, consider anchoring the bubble elsewhere.

Similarly, are there any circumstances in which a particular step might fail to
happen, even if the user performs the action you tell them to? If so, then you
probably need to rethink how your steps work.

## Add an entry point for your Tutorial

Typically this is done through a Tutorial Feature Promo (IPH) or through the
"What's New" Page. For more information on these entry points, check out the
[Getting Started](./getting-started.md) guide.

## Test your Tutorial

If your tutorial launches from an IPH, you can test it any of the ways you could
test an IPH, suggestions are [in the documentation](./feature-promos.md).

You can also launch your Tutorial directly from the tester page
(chrome://internals/user-education). Note that unlike IPH, you do not need to
have the starting point of the tutorial present when you click the "Launch"
button; you merely need to be able to bring it up in 20-30 seconds. The tutorial
will start as soon as the anchor view for the first bubble of the tutorial
becomes visible.

For automated testing, you can:
 - Launch the tutorial directly in an `interactive_ui_tests` test.
 - Launch your IPH (again, see the [IPH documentation](./feature-promos.md)).

In the second case, press the promo's action button to start the Tutorial.

### Automated regression testing

To launch a Tutorial directly in an `interactive_ui_tests` test, invoke the
instance of
[`TutorialService`](/components/user_education/common/tutorial_service.h) you
get from the current profile's
[`UserEducationService`](/chrome/browser/user_education/user_education_service.h)
via `UserEducationServiceFactory::GetForBrowserContext()`.

We recommend using an `InteractiveBrowserTest` (i.e.
[Kombucha](https://goto.google.com/kombucha-playbook)) rather than a vanilla
`InProcessBrowserTest` because of the ability to simulate user input. As you
perform the inputs that trigger the different tutorial steps, you can check for
the expected help bubbles and their contents using the element identifiers
defined in `HelpBubbleView` (if they're Views) or `InstrumentWebContents()` and
`WaitForStateChange()` to find the bubble if it's in a WebUI in a tab.

We also recommend using `PressButton()`, `SelectMenuItem()`, etc. instead of
`MoveMouse()` and `PressMouse()` wherever possible; you should have separate
tests for the responsiveness of your feature's UI; for testing Tutorials the
goal is merely to ensure that each bubble appears as expected.

## Any additional questions?

Please reach out to [Frizzle Team](frizzle-team@google.com) for more
information.

