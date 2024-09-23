# Feature Promos / In-Product Help (IPH)

Once you have the PRD spec for your promo, here are the steps you will follow to
create the promo:
1. Declare your IPH feature and add required Feature Engagement boilerplate
2. Register your IPH with User Education, including type, anchor location, and
   any localizable strings
3. Put hooks in your code to trigger the IPH and to record when the associated
   feature is used
4. Write tests for your triggering and used conditions
5. Roll your promo out to users

## Declare your IPH feature and add required Feature Engagement boilerplate

For historical reasons, and for compatibility with other non-desktop platforms,
Desktop User Education requires you to create your IPH feature under
`/components/feature_engagement`. This is required boilerplate for every promo.

The required changes are:
 - Add your feature to
   [`public/feature_constants.h`](/components/feature_engagement/public/feature_constants.h)
   and [`.cc`](/components/feature_engagement/public/feature_constants.cc).
    - Note that IPH for new features should be disabled by default so they can
      be rolled out with the feature.
    - A new nudge for an existing feature may start enabled by default.
 - Add your feature to the lists in
   [`public/feature_list.h`](/components/feature_engagement/public/feature_list.h)
   and [`.cc`](/components/feature_engagement/public/feature_list.cc).

Required additions in `/tools/metrics`:
 - Add your feature to
   [`actions/actions.xml`](/tools/metrics/actions/actions.xml)
 - Add your feature to
   [`histograms/metadata/feature_engagement/histograms.xml`](/tools/metrics/histograms/metadata/feature_engagement/histograms.xml)

> Ensure you are reproducing the ***name*** of your feature, as defined in
`feature_constants.cc`, any time you need to express your feature as a string.
We have several tests that attempt to check for typos, but they're not perfect.

In addition to failing consistency check tests, neglecting to put your IPH in
the histogram and action files will result in you missing out on critical
telemetry and preclude you from using the User Education 2.0 metrics dashboards.

### Feature Engagement feature configuration

Historically, you would need to add a configuration to either
`components/feature_engagement/public/feature_configurations.cc` or your Finch
configuration, or both. However, Desktop User Education now auto-configures your
IPH for you, and will overwrite most configuration parameters.

> You are strongly recommended ***not*** to add an entry for Desktop Chrome in
this file! At best, it will be overwritten, and at worst it can break your IPH
and possibly other IPH as well.

If you are planning on using the same IPH feature on iOS or Android, you can
(and should) put a configuration in this file, but surrounded by a build
directive, e.g. `#if BUILDFLAG(IS_IOS)`. Note that even if you were to include a
desktop configuration, the values required to "play nice" with Desktop User
Education are different from those recommended on mobile, so you would still
need a separate mobile configuration.

## Register your IPH with User Education

All of these changes will go in `/chrome/browser/ui`.

Add your feature to `MaybeRegisterChromeFeaturePromos()` in
[`views/user_education/browser_user_education_service.cc`](/chrome/browser/ui/views/user_education/browser_user_education_service.cc).
There are many examples already present, which you can use as a model.

Useful things to know:
 - Please use `SetMetadata()` to set accurate metadata.
    - This is how we know who owns a user education experience, when it was
      added, and allows us to provide information on our internal tester page.
    - This will become mandatory in the future.
 - Specifying `HelpBubbleArrow::kNone` will cause your help bubble to float
   below the anchor element without a visible arrow. It is basically only used
   for `kTopContainerElementIdentifier`, which is the View which holds the
   tabstrip, toolbar, and horizontal bookmark bar.
    - This configuration - no arrow, anchored to top container - is used for
      heavyweight IPH that must grab the user's attention; often these are
      important notices. They are by far the most disruptive IPH to users.
 - You must add your localizable strings either in the same CL as the IPH
   registration or in an upstream CL; placeholder strings are not allowed.

### Anchoring your IPH

More information on defining a new help bubble anchor is provided on the
[Help Bubbles](./help_bubbles.md) page.

### Toasts and accessible strings

A "toast" IPH is a small blue bubble with no action buttons (other than a simple
"x" close button). It does not take focus, and disappears after about ten
seconds. Toast bubbles are used to draw attention to new or relocated UI
elements and feature entry points.

When registering a toast, two localizable strings are required: the string to be
displayed, and the string that will be read for screen reader users. These are
intentionally different, and are required to be distinct. The reason for this is
that most toasts have text like "Access [new feature] here", which is fine for
someone who can see where the bubble is, but useless for low-vision users.

A good accessible string not only describes what is being promoted, but also how
to access it, e.g. "Access [new feature] using the [button description] button
on your toolbar". You can also describe the entry point in terms of an
accelerator; see `FeaturePromoSpecification::CreateForToastPromo()` for more
information.

### Custom actions

It's fairly straightforward what Toast, Snooze, and Tutorial IPH do. However,
a Custom Action allows for almost anything to happen when the user clicks the
action button on the IPH. This is handled via a callback which is set when the
promo is registered.

The most common things a custom action can do are:
 - Turn a setting on or off (e.g. Memory Saver).
 - Navigate to a settings page for a particular setting the user might want to
   change.

In the latter case, we recommend but do not require using
[`ShowPromoInPage`](/chrome/browser/ui/user_education/show_promo_in_page.h).
This opens the desired WebUI page and displays a help bubble on a specific
element (typically a specific setting). In order to use this, you will need to
assign an identifier to the anchor in the HTML; see the
[Help Bubbles](./help-bubbles.md) page for more info on help bubble anchors.

> Please note that when you get the callback for the custom action, you will
receive a `FeaturePromoHandle` - this object must remain in scope/undestructed
until you are done with your follow-up actions. Failure to do this may allow
other promos to show over and interfere with those actions.

### Keyed promos

A **keyed promo** is an IPH that can be shown once per unique identifier, which
is typically an App ID, GAIA ID, or some other similar string ID.

When showing a keyed promo you must pass in the key you wish to show it for,
in the `FeaturePromoParams`. A keyed promo is individually dismissed for each
unique key. Make sure when designing such a promo you have a good way of
uniquely identifying each use case via a unique string. You should never
generate unique strings on the fly - they should always be tied to some finite
set of elements associated with the user.

Keyed promos are higher-priority than normal promos and should be used for
privacy/security-related IPH only. They are on an allowlist and must therefore
be approved by the User Education team before they can be registered.

### Restricting IPH to particular platforms or branded Chrome

There are two ways to restrict which platforms an IPH shows on. One is via Finch
configuration; only enabling your IPH feature on the desired targets. The other
is only registering the IPH on supported platforms (or only on branded Chrome),
by using `#if BUILDFLAG()` compiler directives in
`browser_user_education_service.cc`.

Note that if you go the first route, you will get a "feature not enabled" status
in the logs if you attempt to trigger the promo. This is expected, and can be
filtered out by experiment in UMA.

If you go the second route and you try to trigger the IPH, you will get "error"
messages in the histograms and logs, as it is a [non-fatal] error to try to
trigger an unregistered promo. Therefore it is recommended to also gate the
triggering logic in the same way, using the same compiler directives.

> For anything that cannot be expressed with a `BUILDFLAG`, it is recommended
to always register the promo, and simply not show it if it wold not be
appropriate. This creates more consistency in the system and makes it easier to
diagnose problems when an expected IPH does not show.

## Trigger your IPH and record feature usage

To show your IPH call `BrowserWindow::MaybeShowFeaturePromo()`. This will
require having access to a `BrowserWindow`, `BrowserView` (which implements
`BrowserWindow`), or `Browser` object (which owns a `BrowserWindow`).

There are other calls you can make on `BrowserWindow` to e.g. gauge whether a
promo _could_ show or determine whether a promo has been permanently dismissed.
Use them as needed but note that they are not zero-cost so try to avoid calling
them in a tight loop.

When your feature's entry point is used, it is considered good form to note this
so that you don't continue to promote the feature via IPH. You can do this by
calling `BrowserWindow::NotifyPromoFeatureUsed()` or - if you only have access
to the profile and not the browser window at the call site -
`UserEducationService::MaybeNotifyPromoFeatureUsed()`. The feature you should
pass in is the IPH feature.

### Substituting text at runtime

`MaybeShowFeaturePromo()` lets you pass either just a feature or a
`FeaturePromoParams` object. The latter allows for substituting text in the
body, title, or screenreader strings. There are several options, including a
single string, multiple strings, and singular/plural.

In order to use these substitutions, you will need to have matching
substitution fields in the localizable strings defined in your `.grd[p]` files.

### Restrictions placed on when IPH can show

Because of the sheer number of promotions that feature teams put into Chrome,
and the potential for significant disruption for users, great care has been
taken to limit the number and frequency of certain promotions.

> We are enumerating these restrictions here because there are no exceptions. 
If triggering logic causes an IPH to run afoul of one or more of these
restrictions and the IPH is not shown (or is shown less than would be desired),
it is up to the feature developer to rework the triggering logic and/or to
adjust their expectations.

Restrictions placed on all IPH:
 - Only one IPH can show at a time.
 - IPH cannot overlap an open omnibox.
 - IPH cannot show over higher-priority user messaging.
 - An IPH that has been actively dismissed (not snoozed) by the user cannot
   show again.
 - A "toast" IPH that is displayed for the full period and times out cannot
   show again.
 - An IPH where the user has already engaged the entry point a minimum number
   of times cannot show thereafter.

Restrictions on "heavyweight" IPH (Snooze, Tutorial, Custom Action) - note that
these only apply to normal-priority promos:
 - Cannot show in the first 10 minutes after the start of a browser session.
 - Cannot show within several days of another heavyweight IPH.

Future planned restrictions on all IPH:
 - Will be delayed until after user stops typing/clicking/navigating. This will
   not prevent the IPH from showing in most cases; only postpone it.

### Triggering best practices

You should ideally only call `MaybeShowFeaturePromo()` in response to specific
user actions, UI changes, or events in the system.

For example, when the user opens the _Nth_ tab and has never created a tab
group, show the Tab Groups IPH. When the autofill dialog pops up, show the "we
can now suggest autofill for unlabeled fields" IPH. When free memory reaches
some minimum threshold, show the Memory Saver IPH.

"Toast" promos can also be shown immediately to indicate that some UI element
has moved. For example, it is okay to show a "your side panels can now be found
here" IPH at startup _if the user was regularly using the side panel_.
(Certain legal notices are also required to show at startup.)

Avoid calling `MaybeShowFeaturePromo()` over and over, or preemptively before
the user has taken an action that suggests they would benefit from the feature
being promoted. If you want to promote a new feature at startup, consider
instead putting it on the "What's New" Page, or using a "New" Badge, or both.
See [Getting Started](./getting-started.md) for links to these other options.

### Triggering a promo at startup

If your promo is eligible to show at startup (see section above) then you can
use `BrowserWindow::MaybeShowStartupFeaturePromo()`, which you should call ASAP
during startup. Your promo will be displayed when ready, unless preempted by
higher-priority messaging, and you can opt to get a callback with the result.

This cannot effectively be used with heavyweight, normal-priority IPH, which
should never show at startup.

## Test your feature promo

There are three ways to test a promo manually, and one preferred way to write
regression tests for your promo.

### Manual approach 1: Fresh Profile, Cooldowns Disabled

Run chrome with a fresh profile (`--user-data-dir=...`) and (if your IPH is
heavyweight and normal-priority) with User Education rate-limiting disabled
(`--disable-user-education-rate-limiting`). Perform the steps that should
trigger your IPH. Verify it shows when it should, and then verify that it does
not reshow when dismissed or after your feature is used.

If you want to see or clear the data around your IPH feature (including whether
it has been permanently dismissed) go to `chrome://internals/user-education`,
find your feature, and expand the trays underneath to see and optionally clear
the recorded data. Clearing the data can allow you to trigger your feature over
and over on the same profile.

### Manual approach 2: Demo Mode

Demo mode is an older mode that bypasses most checks for whether a promo can
be displayed. Go to `chrome://flags/#in-product-help-demo-mode-choice` and
select your promo, then restart Chrome. This will eliminate any restriction on
reshowing your promo, so you may trigger it any number of times. Using your
feature's entry point will have no effect on the promo.

This approach is not preferred because it sidesteps logic you might want to
test. But it is useful if all you want to do is ensure that your triggering
logic is getting called in the right place.

### Manual approach 3: Tester Page

If you just want to preview what your help bubble will look like, you can go to
the tester page at `chrome://internals/user-education`, find your promo, and
click the "Launch" button. This will attempt to show your IPH in the current UI.
Note that if your IPH anchors to an element that is not present, it will not be
able to show.

This approach is useful when you want to verify the appearance of your help
bubble, how the text, title, image, and buttons will look, etc. However it is of
very limited application and does not verify that your IPH will actually show
when you want it to.

### Interaction-testing your IPH

To test your IPH, use `InteractiveFeaturePromoTest` as your test's base class.
If you already have a test class for your feature you want to inherit from that
derives from `InProcessBrowserTest`, use
`InteractiveFeaturePromoTestT<YourTestClass>` instead.

Then, write a [Kombucha](https://goto.google.com/kombucha-playbook) test which
performs the steps that would trigger your IPH, with a `WaitForPromo` to verify
your promo is shown.

Interactive tests should almost always be in `interactive_ui_tests`, not
`browser_tests`. An `InteractiveFeaturePromoTest` can be in `browser_tests` only
so long as  nothing in the tests would break if the browser window or any piece
of secondary UI (such as a dialog or menu) were to randomly lose focus.

`browser_tests` does not guarantee that the browser is the only foreground
program on the test machine, and many dialogs and menus in Chrome disappear on
loss of focus.

## Roll your promo out to users

You need only enable your IPH feature (declared in `feature_constants.h`) in
your Finch configuration and in `fieldtrial_testing_config.json`, as with any
other feature.

You can roll the IPH feature out alongside the feature it is promoting, or you
can A/B test the IPH within the arm of your study where the promoted feature is
rolling out to gauge the success of your User Education campaign.

In general, it is safe to just roll them out together. Also, try to avoid having
a situation where a user has the IPH but not the promoted feature; this usually
won't cause errors, but will result in a lot of pollution in the UMA dashboards.

For IPH that are nudging users to do something more efficiently with an existing
(i.e. fully rolled out to stable) feature, the same guidelines apply, except
that there is no promoted feature to roll out; you will still need to do a
normal rollout of the promo feature.
