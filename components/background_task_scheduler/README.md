# Quick Start Guide to using BackgroundTaskScheduler

## Background

In Android M+ it is encouraged to use `JobScheduler` for all background jobs,
instead of using things like `IntentService` or polling using alarms. Using the
system API is beneficial as it has a full view of what goes on in the system and
can schedule jobs accordingly.

However, that leaves an API gap for Android L and below. Prior to Android L, the
`JobScheduler` API was not available at all. It was introduced in Android L; but
is not recommended on that platform, because it limits task execution time to 1
minute. This is not really practically usable. For example, merely setting up a
network connection will often burn through much of that budget. Android M+
extends this execution time limit to 10 minutes.

For these older platforms, we can leverage the GcmNetworkManager API provided by
Google Play services to implement a suitable replacement for the JobScheduler
API. The `background_task_scheduler` component provides a new framework for use
within Chromium to schedule and execute background jobs using the frameworks
available on a given version of Android. The public API of the framework is
similar to that of the Android `JobScheduler`, but it is backed by either the
system `JobScheduler` API or by GcmNetworkManager. What service is used to back
the framework remains a black box to callers of the API.

In practice, we prefer to use system APIs, since they do not require including
external libraries, which would bloat the APK size of Chrome and add unnecessary
complexity. Thus, the GcmNetworkManager is only used when the system API is not
available (or available but not considered stable enough). That is, the
JobScheduler API is used on Android M+; and the GcmNetworkManager is used
otherwise.

> NOTE: Some of the pre-M devices do not include Google Play services and
> therefore remain unsupported by `background_task_scheduler`.
> Ultimately, this component hopes to provide a full compatibility
> layer on top of `JobScheduler`. However, until that is implemented, please be
> thoughtful about whether this component provides the coverage that your
> background task needs.

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

There are two main types of tasks; one-off tasks and periodic tasks. One-off
tasks are only executed once, whereas periodic tasks are executed once per
a defined interval.

There are two steps in the process of creating a TaskInfo:

 1. the specific timing info is created; there are two objects available - `OneOffInfo` and
 `PeriodicInfo`; each one of these objects has its own builder;
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
[implementation](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/background_task_scheduler/NativeBackgroundTask.java)
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