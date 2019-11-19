# Feature Engagement

The Feature Engagement component provides a client-side backend for displaying
feature enlightenment or in-product help (IPH) with a clean and easy to use API
to be consumed by the UI frontend. The backend behaves as a black box and takes
input about user behavior. Whenever the frontend gives a trigger signal that
in-product help could be displayed, the backend will provide an answer to
whether it is appropriate to show it or not.

[TOC]

## Objectives

We often add new features, but some are hard to find. Both new and old features
could benefit from being surfaced for users that we believe would be the ones
who benefit the most. This has lead to the effort of providing direct in-product
help to our end users that should be extremely context aware to maximize the
value of the new information.

Conceptually one could implement tracking whether In-Product Help should be
displayed or not through a single preference for whether it has been shown
before. However, that leads to a few issues that this component tries to solve:

*   Make showing In-Product Help context aware.
    *   If a user is continuously using a feature, there is no reason for Chrome
        to display In-Product Help for it.
    *   Other events might be required to have happened first that would make it
        more likely that the end user would be surprised and delighted when the
        In-Product Help in fact does show up.
        *   Example: Having seen the Chrome offline dino 10 times the last week,
            the user might be happier if they are informed that they can
            download web pages exactly as a page successfully loads.
*   Tackle interactions between different In-Product Help features.
    *   If other In-Product Help has been shown within the current session, we
        might not want to show a different one.
    *   Whether we have shown a particular In-Product Help or not might be a
        precondition for whether we should show different one.
*   Users should be able to use try out a feature on their own for some time
    before they see help.
    *   We should show In-Product Help only if they don't seem use it, but we
        believe it would be helpful to them.
*   Share the same statistics framework across all of Chrome.
    *   Sharing a framework within Chrome makes it easier to track statistics
        and share queries about In-Product Help in a common way.
*   Make it simpler to add new In-Product Help for developers, but still
    enabling them to have a complex decision tree for when to show it.

## Overview

Each In-Product Help is called a feature in this documentation. Every feature
will have a few important things that are tracked, particularly whether the
in-product help has been displayed, whether the feature the IPH highlights has
been used and whether any required preconditions have been met. All of these are
tracked within **daily buckets**. This tracking is done only
**locally on the device** itself.

The client-side backend is feature agnostic and has no special logic for any
specific features, but instead provides a generic API and uses the Chrome
Variations framework to control how often IPH should be shown for end users. It
does this by setting thresholds in the experiment params and compare these
numbers to the local state.

Whenever the triggering condition for possibly showing IPH happens, the frontend
asks the backend whether it should display the IPH. The backend then
compares the current local state with the experiment params to see if they are
within the given thresholds. If they are, the frontend is informed that it
should display the IPH. The backend does not display any UI.

To ensure that there are not multiple IPHs displayed at the same time, the
frontend also needs to inform the backend whenever the IPH has been dismissed.

In addition, since each feature might have preconditions that must be met within
the time window configured for the experiment, the frontend needs to inform the
backend whenever such events happen.

To ensure that it is possible to use whether a feature has been used or
not as input to the algorithm to decide whether to show IPH and for tracking
purposes, the frontend needs to inform whenever the feature has been used.

Lastly, some preconditions might require something to never have happened.
The first time a user has IPH available, that will typically be true, since
the event was just started being tracked. Therefore, the first time the
Chrome Variations experiment is made available to the user, the date is tracked
so it can be used to require that the IPH must have been available for at least
`N` days.

The backend will track all the state in-memory and flush it to disk when
necessary to ensure the data is consistent across restarts of the application.
The time window for how long this data is stored is configured server-side.

All of the local tracking of data will happen per Chrome user profile, but
everything is configured on the server side.

## Developing a new In-Product Help Feature

You need to do the following things to enable your feature, all described in
detail below.

*   [Declare your feature](#Declaring-your-feature) and make it available to the
    `feature_engagement::Tracker`.
*   [Start using the `feature_engagement::Tracker` class](#Using-the-feature_engagement_Tracker)
    by notifying about events, and checking whether In-Product Help should be
    displayed.
*   [Configure UMA](#Configuring-UMA).
*   [Add a local field trial testing configuration](#Adding-a-local-field-trial-testing-configuration).

### Declaring your feature

You need to create a `base::Feature` that represents your In-Product Help
feature, that enables the whole feature to be controlled server side.
The name should be on the form:

1.   `kIPH` prefix
1.   Your unique CamelCased name, for example `MyFun`.
1.   `Feature` suffix.

The name member of the `base::Feature` struct should match the constant name,
and be on the form:

1.   `IPH_` prefix
1.   Your unique CamelCased name, for example `MyFun`.

There are also a few more places where the feature should be added, so overall
you would have to add it to the following places:

*   `//components/feature_engagement/public/feature_constants.cc`:

    ```c++
    const base::Feature kIPHMyFunFeature{"IPH_MyFun",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
    ```

*   `//components/feature_engagement/public/feature_constants.h`:

    ```c++
    extern const base::Feature kIPHMyFunFeature;
    ```

*   `//components/feature_engagement/public/feature_list.cc`:
    *   Add to `const base::Feature* kAllFeatures[]`.
*   `//components/feature_engagement/public/feature_list.h`:
    *   `DEFINE_VARIATION_PARAM(kIPHMyFunFeature, "IPH_MyFun");`
    *   `VARIATION_ENTRY(kIPHMyFunFeature)`

If the feature will also be used from Java, also add it to:
`org.chromium.components.feature_engagement.FeatureConstants` as a
`String` constant.

### Using the feature_engagement::Tracker

To retrieve the `feature_engagement::Tracker` you need to use your platform
specific way for how to retrieve a `KeyedService`. For example for desktop
platforms and Android, you can use the `feature_engagement::TrackerFactory` in
`//chrome/browser/feature_engagement/tracker_factory.h`
to retrieve it from the `Profile` or `BrowserContext`:

```c++
feature_engagement::Tracker* tracker =
    feature_engagement::TrackerFactory::GetForBrowserContext(profile);
```

That service can be first of all used to notify the backend about events:

```c++
tracker->NotifyEvent("your_event_name");
```

In addition, it can tell you whether it is a good time to trigger the help UI:

```c++
bool trigger_help_ui =
    tracker->ShouldTriggerHelpUI(feature_engagement::kIPHMyFunFeature);
if (trigger_help_ui) {
    // Show IPH UI.
}
```

If `feature_engagement::Tracker::ShouldTriggerHelpUI` return `true` you must
display the In-Product Help, as it will be tracked as if you showed it. In
addition you are required to inform when the feature has been dismissed:

```c++
tracker->Dismissed(feature_engagement::kIPHMyFunFeature);
```

#### Inspecting whether IPH has already been triggered for a feature

Sometimes additional tracking is required to figure out if in-product help for a
particular feature should be shown, and sometimes this is costly. If the
in-product help has already been shown for that feature, it might not be
necessary any more to do the additional tracking of state.

To check if the triggering condition has already been fulfilled (i.e. can not
currently be triggered again), you can call:

```c++
// TriggerState is { HAS_BEEN_DISPLAYED, HAS_NOT_BEEN_DISPLAYED, NOT_READY }.
Tracker::TriggerState trigger_state =
    GetTriggerState(feature_engagement::kIPHMyFunFeature);
```

Inspecting this state requires the Tracker to already have been initialized,
else `NOT_READY` is always returned. See `IsInitialized()` and
`AddOnInitializedCallback(...)` for how to ensure the call to this is delayed.

##### A note about TriggerState naming

Typically, the `FeatureConfig` (see below) for any particular in-product help
requires the configuration for `event_trigger` to have a comparator value of
`==0`, i.e. that it is a requirement that the particular in-product help has
never been shown within the search window. The values of the `TriggerState` enum
reflects this typical usage, whereas technically, this is the correct
interpretation of the states:

*   `HAS_BEEN_DISPLAYED`: `event_trigger` condition is NOT met and in-product
    help will not be displayed if `Tracker` is asked.
*   `HAS_NOT_BEEN_DISPLAYED`: `event_trigger` condition is met and in-product
    help might be displayed if `Tracker` is asked.
*   `NOT_READY`: `Tracker` not fully initialized yet, so it is unable to
    inspect the state.

#### Inspecting whether IPH would have been triggered for a feature

Another way to check the internal state of the `Tracker` is to invoke
`feature_engagement::Tracker::WouldTriggerHelpUI` which is basically the same as
invoking `feature_engagement::Tracker::ShouldTriggerHelpUI`, but being allowed
to ignore the state. It is still required to invoke
`feature_engagement::Tracker::ShouldTriggerHelpUI` if in-product help should be
shown.

> **WARNING: It is not guaranteed that invoking `ShouldTriggerHelpUI(...)`
> after this would yield the same result.** The state might change
> in-between the calls because time has passed, other events might have been
> triggered, and other state might have changed.

### Configuring UMA

To enable UMA tracking, you need to make the following changes to the metrics
configuration:

1.  Add feature to the histogram suffix `IPHFeatures` in:
    `//tools/metrics/histograms/histograms.xml`.
    *   The suffix must match the `base::Feature` `name` member of your feature.
1.  Add feature to the actions file at: `//tools/metrics/actions/actions.xml`.
    *   The suffix must match the `base::Feature` `name` member.
    *   Find the `<action-suffix>` entry at the end of the file, where the
        following `<affected-action>`s are listed:
        *   `InProductHelp.NotifyEvent.IPH`
        *   `InProductHelp.NotifyUsedEvent.IPH`
        *   `InProductHelp.ShouldTriggerHelpUI.IPH`
        *   `InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.IPH`
        *   `InProductHelp.ShouldTriggerHelpUIResult.Triggered.IPH`
        *   `InProductHelp.ShouldTriggerHelpUIResult.WouldHaveTriggered.IPH`
    *   Add an alphebetically sorted entry to the list of `<suffix>`es like:
        `<suffix name="MyFunFeature" label="For MyFunFeature feature."/>`

### Adding a local field trial testing configuration

For each in-product help feature, it is required to also configure the expected
launch configuration as the main testing configuration. See
[Field Trial Testing Configuration][field-trial-testing-configuration] for
details.

Basically this requires you to add a new section to
`//testing/variations/fieldtrial_testing_config.json` for your feature. The
format is described in the documentation linked above, but it will probably
look something like this:

```javascript
{
  "MyFunFeatureStudy": [
    {
      "platforms": ["android"],
      "experiments": [
        {
          "name": "MyFunFeatureLaunchConfig",
          "params": {
            "availability": ">=30",
            "session_rate": "<1",
            "event_used": "name:fun_event_happened;comparator:any;window:360;storage:360",
            "event_trigger": "name:fun_feature_iph_triggered;comparator:any;window:360;storage:360",
            "event_1": "name:related_fun_thing_happened;comparator:>=1;window:360;storage:360"
          },
          "enable_features": ["IPH_MyFunFeature"],
          "disable_features": []
        }
      ]
    }
  ],
  ...
}
```

## Demo mode

The feature_engagement::Tracker supports a special demo mode, which enables a
developer or testers to see how the UI looks like without using Chrome
Variations configuration.

The demo mode behaves differently than the code used in production where the
chrome Variations configuration is used. Instead, it has only a few rules:

*   Event model must be ready (happens early).
*   No other features must be showing at the moment.
*   The given feature must not have been shown before in the current session.

This basically leads to each selected IPH feature to be displayed once. The
triggering condition code path must of course be triggered to display the IPH.

How to select a feature or features is described below.

### Enabling all In-Product Help features in demo-mode

1.  Go to chrome://flags
1.  Find "In-Product Help Demo Mode" (#in-product-help-demo-mode-choice)
1.  Select "Enabled"
1.  Restart Chrome

### Enabling a single In-Product Help feature in demo-mode

1.  Go to chrome://flags
1.  Find “In-Product Help Demo Mode” (#enable-iph-demo-choice)
1.  Select the feature you want with the "Enabled " prefix, for example for
    `IPH_MyFunFeature` you would select:
    *   Enabled IPH_MyFunFeature
1.  Restart Chrome

## Using Chrome Variations

Each In-Product Help feature must have its own feature configuration
[FeatureConfig](#FeatureConfig), which has 4 required configuration items that
must be set, and then there can be an arbitrary number of additional
preconditions (but typically on the order of 0-5).

The data types are listed below.

### FeatureConfig

Format:

```
{
  "availability": "{Comparator}",
  "session_rate": "{Comparator}",
  "session_rate_impact": "{SessionRateImpact}",
  "event_used": "{EventConfig}",
  "event_trigger": "{EventConfig}",
  "event_???": "{EventConfig}",
  "tracking_only": "{Boolean}"
  "x_???": "..."
 }
```

The `FeatureConfig` fields `availability`, `session_rate`, `event_used` and
`event_trigger` are required, and there can be an arbitrary amount of other
`event_???` entries.

*   `availability` __REQUIRED__
    *   For how long must an in-product help experiment have been available to
        the end user.
    *   The value of the `Comparator` is in a number of days.
    *   See [Comparator](#Comparator) below for details.
*   `session_rate` __REQUIRED__
    *   How many other in-product help have been displayed within the current
        end user session.
    *   The value of the `Comparator` is a count of total In-Product Help
        displayed in the current end user session.
    *   See [Comparator](#Comparator) below for details.
*   `session_rate_impact`
    *   Which other in-product help features showing the current IPH impacts.
    *   By default, a feature impacts every other feature.
    *   Defaults to `all`.
    *   See [SessionRateImpact](#SessionRateImpact) below for details.
*   `event_used` __REQUIRED__
    *   Relates to what the in-product help wants to highlight, i.e. teach the
        user about and increase usage of.
    *   This is typically the action that the In-Product Help should stimulate
        usage of.
    *   Special UMA is tracked for this.
    *   See [EventConfig](#EventConfig) below for details.
*   `event_trigger` __REQUIRED__
    *   Relates to the times in-product help is triggered.
    *   Special UMA is tracked for this.
    *   See [EventConfig](#EventConfig) below for details.
*   `event_???`
    *   Similar to the other `event_` items, but for all other preconditions
        that must have been met.
    *   Name must match `/^event_[a-zA-Z0-9-_]+$/` and not be `event_used` or
        `event_trigger`.
    *   See [EventConfig](#EventConfig) below for details.
*   `tracking_only`
    *   Set to true if in-product help should never trigger.
    *   Tracker::ShouldTriggerHelpUI(...) will always return false, but if all
        other conditions are met, it will still be recorded as having been
        shown in the internal database and through UMA.
    *   This is meant to be used by either local tests or for comparisons
        between different experiment groups.
    *   If you want to later transition users with this flag set to `true` to
        in fact display in-product help, you might want to use a different
        `EventConfig::name` for the `event_trigger` configuration than the
        non-tracking configuration.
    *   Defaults to `false`.
    *   See [Boolean](#Boolean) below for details.
*   `x_???`
    *   Any parameter starting with `x_` is ignored by the feature engagement
        tracker.
    *   A typical use case for this would be if there are multiple experiments
        for the same in-product help, and you want to specify different strings
        to use in each of them, such as:

        ```
        "x_promo_string": "IDS_MYFUN_PROMO_2"
        ```

    *   Failing to use an `x_`-prefix for parameters unrelated to the
        `FeatureConfig` will end up being recorded as `FAILURE_UNKNOWN_KEY` in
        the `InProductHelp.Config.ParsingEvent` histogram.

**Examples**

```
{
  "availability": ">=30",
  "session_rate": "<1",
  "event_used": "name:download_home_opened;comparator:any;window:90;storage:360",
  "event_trigger": "name:download_home_iph_trigger;comparator:any;window:90;storage:360",
  "event_1": "name:download_completed;comparator:>=1;window:120;storage:180"
}
```

### EventConfig

Format: ```name:{std::string};comparator:{COMPARATOR};window:{uint32_t};storage:{uint32_t}```

The EventConfig is a semi-colon separate data structure with 4 key-value pairs,
all described below:

*   `name`
    *   The name (unique identifier) of the event.
    *   Must match what is used in client side code.
    *   Must only contain alphanumeric, dash and underscore.
        *   Specifically must match this regex: `/^[a-zA-Z0-9-_]+$/`
    *   Value client side data type: std::string
*   `comparator`
    *   The comparator for the event. See [Comparator](#Comparator) below.
*   `window`
    *   Search for this occurrences of the event within this window.
    *   The value must be given as a number of days.
    *   For value N, the following holds:
        *   `0` Nothing should be counted.
        *   `1` |current_day| should be counted.
        *   `2+` |current_day| plus |N-1| more days should be counted.
    *   Value client side data type: uint32_t
*   `storage`
    *   Store client side data related to events for this event minimum this
        long.
    *   The value must be given as a number of days.
    *   For value N, the following holds:
        *   `0` Nothing should be stored.
        *   `1` |current_day| should be stored.
        *   `2+` |current_day| plus |N-1| more days should be stored.
    *   The value should not exceed 10 years (3650 days).
    *   Value client side data type: uint32_t
    *   Whenever a particular event is used by multiple features, the maximum
        value of all `storage` is used as the storage window.

**Examples**

```
name:user_opened_app_menu;comparator:==0;window:14;storage:90
name:user_has_seen_dino;comparator:>=5;window:30;storage:360
name:user_has_seen_wifi;comparator:>=1;window:30;storage:180
```

### Comparator

Format: ```{COMPARATOR}[value]```

The following comparators are allowed:

*   `<` less than
*   `>` greater than
*   `<=` less than or equal
*   `>=` greater than or equal
*   `==` equal
*   `!=` not equal
*   `any` always true (no value allowed)

Other than `any`, all comparators require a value.

**Examples**

```
>=10
==0
any
<15
```

### Boolean

Format: ```[true|false]```

The following values are allowed:

*   `true`
*   `false`

The value must be quoted (like all the other values).

**Examples**

```
true
false
TRUE
FALSE
True
False
```

### SessionRateImpact

Format: ```[all|none|comma-separated list]```

*   `all` means this feature impacts every other feature regarding their
    `session_rate` calculations. This is the default.
*   `none` means that this feature does not impact any other features regarding
    the `session_rate`. This feature may therefore be shown an unlimited amount
    of times, without making other features go over their `session_rate` config.
*   `[comma-separated list]` means that this feature only impacts the particular
    features listed. Use the `base::Feature` name of the feature in the list.
    For features in the list, this feature will affect their `session_rate`
    conditions, and for features not in the list, this feature will not affect
    their `session_rate` calculations.
    *   It is *NOT* valid to use the feature names `all` or `none`. They must
        only be used alone with no comma, at which point they work as described
        above.

**Examples**

```
all
none
IPH_DownloadHome
IPH_DonwloadPage,IPH_DownloadHome
```

### Using Chrome Variations at runtime

It is possible to test the whole backend from parsing the configuration,
to ensuring that help triggers at the correct time. To do that
you need to provide a JSON configuration file, that is then
parsed to become command line arguments for Chrome, and after
that you can start Chrome and verify that it behaves correctly.

1.  Create a file which describes the configuration you are planning
    on testing with, and store it. In the following example, store the
    file `DownloadStudy.json`:

    ```javascript
    {
      "DownloadStudy": [
        {
          "platforms": ["android"],
          "experiments": [
            {
              "name": "DownloadExperiment",
              "params": {
                "availability": ">=30",
                "session_rate": "<1",
                "event_used": "name:download_home_opened;comparator:any;window:90;storage:360",
                "event_trigger": "name:download_home_iph_trigger;comparator:any;window:90;storage:360",
                "event_1": "name:download_completed;comparator:>=1;window:120;storage:180"
              },
              "enable_features": ["IPH_DownloadHome"],
              "disable_features": []
            }
          ]
        }
      ]
    }
    ```

1.  Use the field trial utility to convert the JSON configuration to command
    line arguments:

    ```bash
    python ./tools/variations/fieldtrial_util.py DownloadStudy.json android shell_cmd
    ```

1.  Pass the command line along to the binary you are planning on running.

    Note: For Android you need to ensure that all arguments are are within one
    set of double quotes. In particular, for the Android target
    `chrome_public_apk` it would be:

    ```bash
    ./out/Debug/bin/chrome_public_apk run --args "--force-fieldtrials=DownloadStudy/DownloadExperiment --force-fieldtrial-params=DownloadStudy.DownloadExperiment:availability/>=30/event_1/name%3Adownload_completed;comparator%3A>=1;window%3A120;storage%3A180/event_trigger/name%3Adownload_home_iph_trigger;comparator%3Aany;window%3A90;storage%3A360/event_used/name%3Adownload_home_opened;comparator%3Aany;window%3A90;storage%3A360/session_rate/<1 --enable-features=IPH_DownloadHome<DownloadStudy"
    ```

### Printf debugging

Several parts of the feature engagement tracker has some debug logging
available. To see if the current checked in code covers your needs, try starting
a debug build of chrome with the following command line arguments:

```bash
--vmodule=tracker_impl*=2,event_model_impl*=2,persistent_availability_store*=2,chrome_variations_configuration*=3
```

## Development of `//components/feature_engagement`

### Testing

To compile and run tests, assuming the product out directory is `out/Debug`,
use:

```bash
ninja -C out/Debug components_unittests ;
./out/Debug/components_unittests \
  --test-launcher-filter-file=components/feature_engagement/components_unittests.filter
```

When adding new test suites, also remember to add the suite to the filter file:
`//components/feature_engagement/components_unittests.filter`.

[field-trial-testing-configuration]: https://chromium.googlesource.com/chromium/src/+/master/testing/variations/README.md
