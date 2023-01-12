# Quick Start Guide to using BackgroundTaskScheduler

## Overview

This document describes how to schedule a background task in Android using
BackgroundTaskScheduler API. Although most of the examples are described in
Java, the exact same API is available in C++ as well.

## Background

In Android it is encouraged to use `JobScheduler` for all background jobs,
instead of using things like `IntentService` or polling using alarms. Using the
system API is beneficial as it has a full view of what goes on in the system and
can schedule jobs accordingly.

The `background_task_scheduler` component provides a framework for use within
Chromium to schedule and execute background jobs using the system API. The
API of the framework is similar to that of the Android `JobScheduler`.

## What is a task

A task is defined as a class that implements the `BackgroundTask` interface,
which looks like this:

```java
interface BackgroundTask {
  interface TaskFinishedCallback {
    void taskFinished(boolean needsReschedule);
  }

  boolean onStartTask(Context context,
                      TaskParameters taskParameters,
                      TaskFinishedCallback callback);
  boolean onStopTask(Context context,
                     TaskParameters taskParameters);
}
```

**Any class implementing this interface must have a public constructor which takes
no arguments.**

A task must also have a unique ID, and it must be listed in `TaskIds` to ensure
there is no overlap between different tasks.

The connection between `TaskIds` and the corresponding `BackgroundTask` classes is done by injecting
a `BackgroundTaskFactory` class in `BackgroundTaskSchedulerFactory`. For the //chrome embedder
(which is the only one needing the association), the `ChromeBackgroundTaskFactory` [implementation]
(https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser
/background_task_scheduler/ChromeBackgroundTaskFactory.java) was created. Anyone that adds a new
task id to `TaskIds` should add a case in this class to.

## How to schedule a task

A task is scheduled by creating an object containing information about the task,
such as when to run it, whether it requires battery, and other similar
constraints. This object is called `TaskInfo` and has a builder you can use
to set all the relevant fields.

There are three main types of tasks; one-off tasks, periodic tasks, and exact timing info tasks. One-off
tasks are only executed once, whereas periodic tasks are executed once per
a defined interval. The exact info tasks are triggered at the exact scheduled time.

There are two steps in the process of creating a TaskInfo:

 1. the specific timing info is created; there are three objects available - `OneOffInfo`,
 `PeriodicInfo`, and `ExactInfo`; each one of these objects has its own builder;
 2. the task info is created using the `createTask` method; other parameters can be set afterwards.

As an example for how to create a one-off task that executes in 200 minutes,
you can do the following:

```java
TaskInfo.TimingInfo oneOffInfo = TaskInfo.OneOffInfo.create()
                                    .setWindowEndTimeMs(TimeUnit.MINUTES.toMillis(200)).build();
TaskInfo taskInfo = TaskInfo.createTask(TaskIds.YOUR_FEATURE,
                            oneOffInfo).build();
```

For a periodic task that executes every 200 minutes, you can call:

```java
TaskInfo.TimingInfo periodicInfo = TaskInfo.PeriodicInfo.create()
                                    .setIntervalMs(TimeUnit.MINUTES.toMillis(200)).build();
TaskInfo taskInfo = TaskInfo.createTask(TaskIds.YOUR_FEATURE,
                            periodicInfo).build();
```

Typically you will also set other required parameters such as what type of
network conditions are necessary and whether the task requires the device to
be charging. They can be set on the builder like this:

```java
TaskInfo.TimingInfo oneOffInfo = TaskInfo.OneOffInfo.create()
                                    .setWindowStartTimeMs(TimeUnit.MINUTES.toMillis(100))
                                    .setWindowEndTimeMs(TimeUnit.MINUTES.toMillis(200)).build();
TaskInfo taskInfo = TaskInfo.createTask(TaskIds.YOUR_FEATURE,
                            oneOffInfo)
                          .setRequiresCharging(true)
                          .setRequiredNetworkType(
                            TaskInfo.NETWORK_TYPE_UNMETERED)
                          .build();
```

Note that the task will be run after `windowEndTimeMs` regardless of whether the
prerequisite conditions are met. To work around this, mark the `windowEndTimeMs`
to `Integer.MAX_VALUE`.

When the task is ready for scheduling, you use the
`BackgroundTaskSchedulerFactory` to get the current instance of the
`BackgroundTaskScheduler` and use it to schedule the job.

```java
BackgroundTaskSchedulerFactory.getScheduler().schedule(myTaskInfo);
```

If you ever need to cancel a task, you can do that by calling `cancel`, and
passing in the task ID:

```java
BackgroundTaskSchedulerFactory.getScheduler().cancel(TaskIds.YOUR_FEATURE);
```

## Passing task arguments

A `TaskInfo` supports passing in arguments through a `Bundle`, but only values
that can be part of an Android `BaseBundle` are allowed. You can pass them in
using the `TaskInfo.Builder`:

```java
Bundle myBundle = new Bundle();
myBundle.putString("foo", "bar");
myBundle.putLong("number", 1337L);

TaskInfo.TimingInfo oneOffInfo = TaskInfo.OneOffInfo.create()
                                    .setWindowStartTimeMs(TimeUnit.MINUTES.toMillis(100))
                                    .setWindowEndTimeMs(TimeUnit.MINUTES.toMillis(200)).build();
TaskInfo taskInfo = TaskInfo.createTask(TaskIds.YOUR_FEATURE,
                            oneOffInfo)
                          .setExtras(myBundle)
                          .build();
```

These arguments will be readable for the task through the `TaskParameters`
object that is passed to both `onStartTask(...)` and `onStopTask(...)`, by
doing the following:

```java
boolean onStartTask(Context context,
                    TaskParameters taskParameters,
                    TaskFinishedCallback callback) {
  Bundle myExtras = taskParameters.getExtras();
  // Use |myExtras|.
  ...
}
```
For native tasks, the extras are packed into a std::string, It's the caller's
responsibility to pack and unpack the task extras correctly into the std::string.
We recommend using a proto for consistency.

## Performing actions over TimingInfo objects

To perform actions over the `TimingInfo` objects, based on their implementation, the Visitor design
pattern was used. A public interface is exposed for this: `TimingInfoVisitor`. To use this
interface, someone should create a class that would look like this:

```java
class ImplementedActionVisitor implements TaskInfo.TimingInfoVisitor {
  @Override
  public void visit(TaskInfo.OneOffInfo oneOffInfo) { ... }

  @Override
  public void visit(TaskInfo.PeriodicInfo periodicInfo) { ... }
}
```

To use this visitor, someone would make the following calls:

```java
ImplementedActionVisitor visitor = new ImplementedActionVisitor();
myTimingInfo.accept(visitor);
```

## Loading Native parts

Some of the tasks running in the background require native parts of the browser
to be initialized. In order to simplify implementation of such tasks, we provide
a base `NativeBackgroundTask`
[implementation](https://cs.chromium.org/chromium/src/components/background_task_scheduler/android/java/src/org/chromium/components/background_task_scheduler/NativeBackgroundTask.java)
in the browser layer. It requires extending classes to implement 4 methods:

 * `onStartTaskBeforeNativeLoaded(...)` where the background task can decide
   whether conditions are correct to proceed with native initialization;
 * `onStartTaskWithNative(...)` where the background task can be sure that
   native initialization was completed, therefore it can depend on that part of
   the browser;
 * `onStopTaskBeforeNativeLoaded(...)` which is delivered to the background task
   just like `onStopTask(...)` and the native parts of the browser are not
   loaded;
 * `onStopTaskWithNative(...)` which is delivered to the background task just
   like `onStopTask(...)` and the native parts of the browser are loaded.

While in a normal execution, both `onStart...` methods are called, only one of
the stopping methods will be triggered, depending on whether the native parts of
the browser are loaded at the time the underlying scheduler decides to stop the
task.

## Launching Browser process

After the advent of servicfication in chrome, we have the option of launching a
background task in a reduced service manager only mode without the need to
launch the full browser process. In order to enable this, you have to override
`NativeBackgroundTask#supportsMinimalBrowser` and return true or false
depending on whether you want to launch service-manager only mode or full
browser.

## Background processing

Even though the `BackgroundTaskScheduler` provides functionality for invoking
code while the application is in the background, the `BackgroundTask` instance
is still invoked on the application's main thread.

This means that unless the operation is extremely quick, processing must happen
asynchronously, and the call to `onStartTask*(...)` must return before the task
has finished processing. In that case, the method should return once the
asychronous processing has begun, and invoke the `TaskFinishedCallback` when the
processing is finished, which typically happens on a different `Thread`,
`Handler`, or by using an `AsyncTask`.

If at any time the constraints given through the `TaskInfo` object do not hold
anymore, or if the system deems it necessary, `onStopTask*(...)` will be
invoked, requiring all activity to cease immediately. The task can return true
if the task needs to be rescheduled since it was canceled, or false otherwise.
Note that onStopTask*() is not invoked if the task itself invokes
`TaskFinishedCallback` or if the task is cancelled by the caller.

**The system will hold a wakelock from the time
`onStartTaskBeforeNativeLoaded(...)` is invoked until either the task itself
invokes the `TaskFinishedCallback`, or `onStopTask*(...)` is invoked.**
