# Structured Metrics and CrOS Events API

This document describes the client side API for defining and recording Structured Metrics and CrOS Events. Structured Metrics is supported on ChromeOS, Windows, Mac, and Linux. ChromeOS support includes LaCrOS, Ash Chrome, and Platform2 processes.

This is not a technical document.

## Overview
A single element of Structured Metrics is called an Event. These events are defined as a part of a project. Each project is self contained and the events of different projects are unable to be correlated.

## Getting Started
Below are the initial steps that much be take *before* implementation for Structured Metrics and CrOS Events.
1. Complete a project privacy review: Structured Metrics doesn't have a template and [CrOS Events PRD template (Google Only)](https://docs.google.com/document/d/1NywDfiLq0NVy0xyjl68r3ibfovETIguQPLp3-dI2KLo/edit?usp=sharing&resourcekey=0-J2YNtLAZ2r2j35qxqtqSxg).
2. Complete Privacy Review
3. (Optional) Design Doc

## Defining Projects and Events
Projects and Events are declared in [//tools/metrics/structured/sync/structured.xml](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/structured/sync/structured.xml).

### Required Information
* A project has a `name` attribute. All names must be in CamelCase as they are used directly for code generation.
* Each project must have one `owner` element, multiple are allowed.
* An `id` element, valid values: project and none. When value is project then a unique id determined per-project. When value is none the an unique id is not assigned for the events of this project.
* A `scope` element, valid values: profile and device. When value is profile then the unique id is determined per-profile and per-profile. When value is device then the unique id is determined per-device and per-profile. If id is none this element has no meaning. Currently, Windows, Mac, and Linux only support device scope. Scope is not applicable to projects in the ChromiumOS repository.
* A `key-rotation` element. The key rotation describes how often the key used to generate the id is rotated to a new value. E.G. If the key rotations value is 90 then every 90 days the key will be changed resulting in new id's.
* The project, each event, and metric must have a `summary` element. This will be used to describe the purpose of the project and when an event is recorded.
* A series of events are defined. Please see [Event Definition](#event-definition) for more details.
* _(Optional)_ A project has a comma separated list of targets to build for. If not defined, defaults to `chromium`. If the target `webui` is added, the project will also generate code to be used in WebUI's Renderer process (i.e. Typescript).


### Project Example
```xml
<project name="Navigation" targets="chromium,webui">
  <owner>navigator1@chromium.com</owner>
  <owner>navigation-team@google.com</owner>
  <id>project</id>
  <scope>device</scope>
  <key-rotation>90</key-rotation>
  <summary>
    Users navigation within a page.
  </summary>

  <!--Events-->
</project>
```

### Event Definition
* Each event must have a `name` attribute.
* Event has an optional attribute `force_record` which allows for events to be recorded if UMA is not consented. If you think your use-case needs this functionality contact OWNERs.
* The previously described `summary` element.
* An event can have multiple `metric` elements. Each metric must have a `name` attribute and `type` attribute. Valid types are: hmac-string, raw-string, int, double, int-array, and project defined [Enum](#enums). `int-array` metric type has an additional attribute called `max` which is the max length of the integer array.
* An event must have at least one metric unless it is a part of CrOS Event, see [CrOS Events](#cros-events)

#### Enums
An enum can be defined that can be used by all events of the project it was defined. Multiple enums can be defined per-project. It is recommended that the first element is a reasonable default. The syntax is defined in the [Event Examples](#event-examples) section.

#### Event Examples
```xml
<project name="Navigation">
  <!--Project Metadata-->

  <enum name="Direction">
    <variant value="0">None</variant>
    <variant value="1">Forward</variant>
    <variant value="3">Back</variant>
  </enum>


  <event name="PageTransition">
    <summary>
      Captures when a user transitions page.
    </summary>

    <metric name="PageId" type="int">
      <summary>
        The source page of transition.
      </summary>
    <metric>
    <metric name="TransitionDirection" type="Direction">
      <summary>
        The direction of the transition.
      </summary>
    <metric>
  </event>

  <event name="Impression">
    <summary>
      Impression of the transition.
    </summary>

    <metric name="ElementIds" type="int-array" max="10">
      <summary>
        Elements of source page.
      </summary>
    </metric>
  </event>
</project>
```

## CrOS Events
CrOS Event's is a part of Structured Metrics which provides sequencing information at the time the event was recorded. This allows for events to be sequenced. We use a proxy for timestamp which is the reset id and system uptime. Reset id is manually managed when the device boots.

To declare events to be used in CrOS Events they must be declared in the [CrOSEvents](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/structured/sync/structured.xml) project. The `name` of each event must be `<UseCase>.<EventName>`, e.g. `AppDiscovery.AppInstalled`.

We are working on an alternative method for declaring CrOS Events projects that is more scalable and easier to maintain.

## Client API (C++)
By default, all projects in `structured.xml` will be built for the `chromium` target, meaning each event in the project will have an Event API generated in C++.

### Event API (C++)
At build time, a C++ API is generated from the xml. The project name will become the namespace (`navigation`) and the event name is the event's class name (`PageTransition`). Each metric has a setter method in the format `Set<MetricName>` (`SetPageId`). There are l-value and r-value versions for each metric method.

### Recording (C++)
In order to record an event the `StructuredMetricsClient` singleton is needed, defined in [//components/metrics/structured/structured_metrics_client.h](https://source.chromium.org/chromium/chromium/src/+/main:components/metrics/structured/structured_metrics_client.h).

Recording can be done by:

```cpp
#include "components/metrics/structured/structured_metrics_client.h" // for StructuredMetricsClient
#include "components/metrics/structured/structured_events.h" // for event definitions

// Shorten the namespace if desired.
using nav = metrics::structured::v2::navigation;

ms::StructuredMetricsClient::Record(
  nav::PageTransition()
    .SetPageId(page_id)
    .SetTransitionDirection(nav::Direction::Forward));
```

## Client API (Typescript)
When a project is given a `target` of `webui`, they will have an Event API for each event, generated in Typescript, meant to be used for WebUI. The events are sent from the Renderer process to the Browser process using Mojo. Additionally, there is a recording API that can be used to record the event.

### Event API (Typescript)
At build time, a Typescript API is generated from the xml for each event. Each event will generate a builder class in Typescript. Using the same example for the C++ Event API, we will get the following:
* Builder class name is `<ProjectName>_<EventName>`, e.g. `Navigation_PageTransition`
* Two setter methods generated, `setPageId()` and `setTransitionDirection()`
* One `build()` method

#### Enums in Typescript
Enums are declared at the top level in the generated `structured_events.ts` file. In the above example, we will then get an enum declared like:
```ts
export enum Navigation_Direction {
None = 0,
Forward = 1,
Back = 3
};
```

#### CrOS Events `recorded_time_since_boot`
Since it is not possible to obtain the system uptime when recording the event in Typescript, the system uptime is inferred using the event timestamp, used in the following equation:

```
event_system_uptime = browser_system_uptime - (browser_timestamp - event_timestamp)
```


### Recording (Typescript)
In order to record an event, the `recordStructuredEvent()` function exported by `chrome://resources/ash/common/metrics/structured_metrics_service.js` can be used. It will create the required instance of the `StructuredMetricsServiceInterface`, which is the Mojo interface.

Creating an event and recording it would then look like:
```ts
import {recordStructuredEvent} from 'chrome://resources/ash/common/metrics/structured_metrics_service.js';
import * as StructuredEvents from 'chrome://resources/ash/common/metrics/structured_events.js';

// Construct the event.
let structured_event: any = new StructuredEvents.Navigation_PageTransition()
      .setPageId(page_id)
      .setTransitionDirection(
        StructuredEvents.Navigation_Direction.Forward)
      .build();

// Record the event.
recordStructuredEvent(structured_event);
```

## Local Verification
Structured Metrics has a debug page at `chrome://metrics-internals/structured` that can be used to verify recorded events and their contents. The page must be manually refreshed to see recently recorded events.

<!-- TODO: Expand with image and additional functionality -->

<!-- TODO: Maybe add documentation about basic server side processing -->
