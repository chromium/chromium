# Reactive Android Java programming

[TOC]

## Introduction

**Ever notice how Android methods often come in pairs?** For every `onCreate()`,
there is an `onDestroy()`, for every `onStart()`, there is an `onStop()`. The
Android SDK commonly asks clients to register callbacks or extend base classes
that override pairs of methods that correspond to *reversible changes in state*.

*State* is often expressed in Java code as *mutable variables*. A state changes
when you assign a new value to the variable. If a variable can be one value at
some points and another value at other points, that means there are two states
that the variable can have. Everything that interacts with that variable needs
to work correctly for each state the variable can be in. For example, if an
instance variable is `null` in a class's constructor, and set to a value by some
method in that class, then *every* method that tries to call a method on that
variable needs to check whether the value of the variable is null before
handling it, because there is no guarantee which state the variable is in. You
will see a lot of code that looks like this when using this pattern for
representing state:

```java
if (mFoo != null) {
    mFoo.doSomething();
}
```

Additionally, mutator methods may need to check the state at runtime. For
example, lazy initialization often looks like this:

```java
if (mFoo == null) {
    mFoo = new Foo(...);
}
```

This is not bad in and of itself, if the states are well-defined and it's easy
to reason about the set of possible states by looking at the code. However, it
very, very quickly becomes difficult to reason about states when there are any
of the following:

*   **Multiple methods that can mutate state**. For example, a hypothetical
    `Connection` class that reads and writes data over a socket might disconnect
    on a socket error from any `read()` or `write()` call. That means that
    before any `read()` or `write()` call, the state must be checked. (Real Java
    objects will often use `Exception`s to short-circuit code blocks that enter
    an exceptional state).
*   **Methods that throw a runtime error** or have **undefined behavior when in
    a certain state**. For example, a class with an `initialize()` method may
    have methods that should only be called after `initialize()`, but the
    compiler will not be able to check whether `initialize()` has been called.
    This includes every method that has an `assert` statement on a mutable
    instance variable.
*   Multiple **states that interact with each other**. The number of states that
    independently-mutable variables can take is the *product* of the number of
    states of each of the variables. Often, variables are not strictly
    independent (e.g. the only method that mutates a certain variable also
    mutates another), so some states might be unreachable. However, it's **not
    possible for the compiler to tell you which states are reachable** when
    you're using mutable instance variables, so you have to figure that out
    yourself! This makes it hard to exhaustively come up with unittest cases.

## Motivating Example

Consider this seemingly-simple task: you have two variables, `mA` and `mB`, each
of which could be either `null` or some real value of types `A` and `B`,
respectively. Furthermore, you want to initialize a new variable, `mC` of type
`C`, when the values of `mA` and `mB` are non-null, perhaps because the `C`
constructor takes an `A` and a `B`. Finally, if `mA` or `mB` becomes null again
after creating `mC`, reset `mC` to `null`. Also, you need to invoke a `close()`
method on `mC` whenever `mC` is reset. And if `mA` or `mB` changes while `mC`
exists, you need to call `mC.close()` and re-create `mC` with the new `mA` and
`mB`.

```java
class MyClass {
    private A mA = null;
    private B mB = null;
    private C mC = null;

    public void setA(A a) {
        mA = a;
        recalculateC();
    }

    public void setB(B b) {
        mB = b;
        recalculateC();
    }

    private void recalculateC() {
        // This method is always called when A or B changes, so if C exists, it
        // must first be reset.
        if (mC != null) {
            mC.close();
            mC = null;
        }
        if (mA != null && mB != null) {
            mC = new C(mA, mB);
        }
    }
}
```

This may be fine on its own. But chances are, you will want to do something with
`mC` outside these methods. Every read will have to null-check, there's an
**undocumented but critical requirement** that every write to `mA` and `mB` is
done through `setA()` and `setB()`, and that `recalculateC()` is only called
when `mA` or `mB` is mutating, or else it will implicitly close.

These undocumented dependencies can only be protected against regression by
testing. The compiler will not tell you if you made a mistake, so there must be
unittests covering every possible state change. And in this case, we have two
variables, each with two states that each have two possible state transitions,
so **8 test cases are needed to cover everything**. And this is the **simplest**
case of composing **two** independent nullable mutable variables.

Meanwhile, if you use the `Observable`s framework:

```java

class MyClass {
    private final Controller<A> mA = new Controller<>();
    private final Controller<B> mb = new Controller<>();

    {
        mA.and(mB).subscribe(Both.adapt(C::new));
    }

    public void setA(A a) {
        mA.set(a);
    }

    public void setB(B b) {
        mB.set(b);
    }
}
```

In the instance initializer, we set up a simple state machine with two
`Controller`s, which correspond to the mutable instance variables from the
previous example, and an event that observes the state of the `Controller`s and
invokes some logic on certain state changes.

The `and()` call composes the `mA` and `mB`, returning a new `Observable` that
is only activated when *both* sources are activated, and then deactivated if
*either* source is deactivated. `mA` and `mB` are activated or deactivated by
`set()` and `reset()` calls, respectively. The `set()` method deactivates the
state if the argument is `null` (the `reset()` method can also be used to
deactivate the state).

The `subscribe()` call makes it so that when the composed `Observable` formed by
`mA.and(mB)` is activated, a new `C` object will be created. When deactivated,
that `C` object's `close()` method will be called.

This really does cover all the cases we need. If multiple `set*()` calls are
made, an implicit `reset()` call will be made to the relevant `Controller` and
the `C` object associated with the first scope will be `close()`d.

What's better about this? First, notice we **don't need mutable variables**.
Both `Controller` objects are `final`, and are never `null`. We don't at any
point need to know what state the object is in inside any method
implementations; the `Controller`s and the pipeline set up by the `and()` and
`subscribe()` calls handle that for you.

Second, notice how the concerns of mutating and reacting to state are **cleanly
separated**. The mutator methods `setA()` and `setB()` are concerned only with
their respective `Controller`s, and the lifetime of the `C` object is managed in
one place in the instance initializer.

Finally, the relationship between the `A`, `B`, and `C` objects is
**self-documenting**. In the first approach with mutable variables, to
understand that the lifetime of `C` is associated with the intersection of the
lifetimes of `A` and `B`, one has to examine both setters and trace through
`recalculateC()` from the perspective of both of its call sites. In the second
approach, using `Controller`s, the relationship between `A`, `B`, and `C` is
expressed holistically in one line.

## Observables and Observers

Think of an `Observable` as a *container*, with one very important feature: the
ability to register **observers** that will be notified when the contents of the
container change. The contents of the container at a given time is the *state*
of the `Observable`. The `Observable` base class alone does not expose any state
mutators, but it provides ways to register events that will be invoked when
state changes.

All state transitions of an `Observable` is either an *activation* or a
*deactivation*. An activation refers to putting some data into the container,
and a deactivation represents removing some data from the container. The data
that is contained in an `Observable` is thus called **activation data**.

To register events that should be invoked on these state transitions, we
**subscribe** observers. An `Observer` works by opening a `Scope` when an
activation occurs. If a deactivation occurs, the `Scope` opened by the
`Observer` will be closed. The name **scope** comes from how its lifetime is the
time that the activation data exists within the `Observable`, similar to how
variables are *in scope* when using
[RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) in
languages like C++.

The one-to-one mapping of activations in an `Observable` to opened `Scopes` for
each subscribed `Observer` affords many useful properties. Foremost among these
is that the `Scope` can capture the data it is associated with as an *immutable*
variable: the activation data is the same when it is deactivated as when it was
activated. It also ensures that every deactivation requires an activation to
have occurred first (since you cannot `close()` an object that hasn't been
constructed). The implications of these properties will be explored further in
later sections.

### Registering subscriptions with Observables

To subscribe an `Observer` to an `Observable`, we call `subscribe()`
on the `Observable`. The `subscribe()` method takes a single argument, an
`Observer`, which has an `open()` method that returns a `Scope`. The
`Observer`'s `open()` method is called when the `Observable` activates,
and the resulting `Scope`'s `close()` method is called when the `Observable`
deactivates.

Lambda syntax can be used to easily construct `Observer` objects without
much boilerplate. For instance, if we want to simply log the transitions of an
`Observable`, we might do it like this:

```java
void logStateTransitions(Observable<?> observable) {
    observable.subscribe(x -> {
        Log.d(TAG, "activated");
        return () -> Log.d(TAG, "deactivated");
    });
};
```

This is equivalent to the following, much more verbose version:

```java
void logStateTransitions(Observable<?> observable) {
    observable.subscribe(new Observer<Object>() {
        @Override
        public Scope create(Object x) {
            Log.d(TAG, "activated");
            return new Scope() {
                @Override
                public void close() {
                    Log.d(TAG, "deactivated");
                }
            };
        }
    });
}
```

As you can see, the version that uses lambdas is much more readable, as long as
you understand what an `Observer` is. It can help to think of the `return () ->`
as a separator between what happens when the data is activated and what happens
when the data is deactivated.

Either way, when `logStateTransitions()` is called on an `Observable`,
`"activated"` will be printed to the log when that `Observable` is activated,
and `"deactivated"` will be printed to the log when that `Observable` is
deactivated.

Though the above `Observer` does not use the `x` parameter, normally
`Observer` implementations will use the data that `open()` is given, so
that the behavior of the scope can depend on what data the `Observable` is
activated with.

Say we have an `Observable<String>` and we want to log the data it is activated
with:

```java
void logStateTransitionsWithData(Observable<String> observable) {
    observable.subscribe((String s) -> {
        Log.d(TAG, "activated with data: %s", s);
        return () -> Log.d(TAG, "deactivated");
    });
}
```

### Mutating state with Controllers

The `Observable` interface does not provide any way to directly change the state
of the `Observable`. However, subclasses of `Observable` exist that do provide
mutators. The `Controller` class provides `set()` and `reset()`.

`Controller`s are basically nullable, mutable variables that let you register
callbacks, through the `Observable` interface, that are run when the variable
changes.

With this in mind, the `set()` method on `Controller` is like setting a mutable
variable to a value. The `reset()` method is like setting the mutable variable
to `null`.

Here are some guarantees that `Controller`s provide:

*   Start out in the *deactivated* state.
*   If the parameter of `set()` is non-`null`, it enters the *activated* state.
*   If the parameter of `set()` is `null`, it enters the *deactivated* state.
*   If in the activated state, `reset()` and `set(null)` enter the *deactivated*
    state.
*   If already in the deactivated state, `reset()` and `set(null)` do nothing.
*   If in the activated state with data `data1`, `set(data2)` no-ops if
    `data1.equals(data2)`.
*   If in the activated state, and the new data is not `equal()` to the current
    data, `set()` implicitly deactivates and reactivates with the new data.

As corollaries, any registered `Observer` objects will:

*   have their `open()` methods invoked *exactly once* for each non-`null`
    `set()` call
*   have their resulting `Scope`s `close()`d *exactly once* when `reset()` or
    `set()` to `null`
*   always clean up scopes from previous activations when new activations occur

This means this:

```java
void helloGoodbye(Controller<String> message) {
    message.set("hello");
    message.set("goodbye");
    message.set(null);
}
```

has the same behavior as this:

```java
void helloGoodbye(Controller<String> message) {
    message.set("hello");
    message.reset();
    message.set("goodbye");
    message.reset();
}
```

Essentially, a `Controller` adapts two states with two possible actions each:

*   `deactivated`: `set`, `reset`
*   `activated`: `set`, `reset`

into a well-defined state machine with two states and two transitions:

*   `deactivated`: `set`
*   `activated`: `reset`

... by dropping redundant `reset()` calls and inserting implicit `reset()` calls
between contiguous `set()` calls. This cuts the number of state transitions you
need to worry about in half!

Since `Controller`s implement `Observable`, you can register `Observer`
objects with `subscribe()` the same way as in the previous section, or inject a
`Controller` into any method that takes an `Observable` of the same parametric
type.

Remember that an `Observable` can be thought of as a *container* of data. In
this frame of mind, a `Controller` is a container of *one or zero* instances of
some data type.

### Observables without data

The state of a `Controller<T>` is isomorphic to that of a nullable `T` variable
for all types `T`. But there are many cases where what we really want is a
representation of a boolean state: on or off, active or inactive, and don't need
any activation data.

For these cases, the `org.chromium.chromecast.base.Unit` class is used to denote
the fact that there is no data associated with the controller. The `Unit` type
is inspired by the type of the same name in many functional programming
languages, and represents a type with only one possible instance (aka
`Singleton`).

To make a controller without data, you can therefore use `Controller<Unit>`.
Since `Unit` means "no data," and there's only one way to get a `Unit` instance
(through the `Unit.unit()` method), this maps correctly to the concept of a
mutable boolean value.

Note that because the instance of `Unit` equals itself, calling `set()` on a
`Controller<Unit>` when it is already activated will no-op, making the behavior
of `set(Unit.unit())` and `reset()` symmetric.

Example:

```java
{
    Controller<Unit> onOrOff = new Controller<>();
    onOrOff.subscribe(x -> {
        Log.d(TAG, "on");
        return () -> Log.d(TAG, "off");
    });
    onOrOff.set(Unit.unit()); // Turns on.
    onOrOff.set(Unit.unit()); // Does nothing because it's already on.
    onOrOff.reset(); // Turns off.
    onOrOff.reset(); // Does nothing because it's already off.
}
```

It's common for observers and APIs that refer to `Observable`s with no data to
use `Observable<?>` in their interface. This is an easy way to make an
`Observable`'s data "opaque" to observers, even when the data are not `Unit`.

```java
{
    Controller<Foo> foos = new Controller<>();
    // Subscribers to opaqueFoos will get notified of state changes, but will
    // not be able to access the data through the Foo interface.
    Observable<?> opaqueFoos = foos;
    // The observer can use methods like toString() that exist on all Objects,
    // but cannot use Foo's API.
    subscribe(opaqueFoos, x -> {
        Log.d(TAG, "got Foo: %s", x.toString());
        return () -> Log.d(TAG, "lost Foo: %s", x.toString());
    });
}
```

### Composing Observables with `and()`

In the motivating example, we wanted to invoke a callback once *two* independent
`Observable`s have been activated.

Let's say we have a set of states:

* `A`
* `not A`

With the transitions `not A` **<->** `A`.

And introduce another set of states:

* `B`
* `not B`

With the transitions `not B` **<->** `B`.

We can then describe the *combinations* of those state spaces with four states:

*   `neither`
*   `just A`
*   `just B`
*   `A and B`

...and eight transitions:

*   `neither` **<->** `just A`
*   `neither` **<->** `just B`
*   `just A` **<->** `A and B`
*   `just B` **<->** `A and B`

The `Observable` interface gives us a convenient way to get the `(A and B)`
state with a simple call:

```java
public void logWhenBoth(Observable<A> observableA, Observable<B> observableB) {
    observableA.and(observableB).subscribe(...);
}
```

The `and()` method takes the calling `Observable` and the given other
`Observable` and returns a new `Observable` that is only activated when *both*
input `Observable`s are activated.

One way to think about it is that the `and()` call collapses the three states
`(neither)`, `(just A)`, and `(just B)` into one *deactivated* state, and
treats the state `both` as *activated*. For observers of the `and()`-composition
of states, one needs only worry about the two states, *deactivated* and
*activated*, same as with any other observer.

So how do we get the data in the `subscribe()` call? Let's say we want to log
when both `Observable`s are activated:

```java
public void logWhenBoth(Observable<A> observableA, Observable<B> observableB) {
    observableA.and(observableB).subscribe((Both<A, B> data) -> {
        A a = data.first;
        B b = data.second;
        Log.d(TAG, "both activated: a=" + a + ", b=" + b);
        return () -> Log.d(TAG, "deactivated");
    });
}
```

The type of the activation data for an `and()`-composed `Observable` is `Both`.
The `Both` type has two generic parameters, and `first` and `second` public
fields to access the data it encapsulates. It is essentially a trick to box
multiple values into a single object, so we only ever need `Observer`
interfaces that take a single argument.

Since the `and()` method returns an `Observable`, the result can itself call
`and()`, whose result can itself call `and()`, ad infinitum:

```java
    observableA.and(observableB).and(observableC).and(observableD)...
```

But beware, as the associated type of the `Observable` gets uglier and uglier:

```java
    a.and(b).and(c).and(d).subscribe((Both<Both<Both<A, B>, C>, D> data) -> {
        A aData = data.first.first.first;
        B bData = data.first.first.second;
        C cData = data.first.second;
        D dData = data.second;
        Log.d(TAG, "a=%s, b=%s, c=%s, d=%s", aData, bData, cData, dData);
        return () -> Log.d(TAG, "exit");
    });
```

One one hand, it's kind of neat that you can do that at all. But it does come at
a cost to readability. The compiler can catch you if you mess up the
`first.first.second` chains if the types are different, but it is regrettable
that this much work is required to read the compound data. Some methods for
alleviating this are described below.

### Imposing order dependency with `andThen()`

The composition of state spaces `stateA.and(stateB)` doesn't care if `stateA` or
`stateB` was activated first, so it can be activated by either activating
`stateA` and then `stateB`, or by activating `stateB` and then `stateA`.

This means the state `(A and B)`, extracted by the `and()` method on
`Observable`, is too ambiguous for knowing the *order* of activation. If we want
to know whether `A` was activated before `B`, we must *partition* the state
`(A and B)` into `(A and then B)` and `(B and then A)`. The time-dependent state
space for two boolean variables looks like this, with five states:

*   `neither`
*   `just A`
*   `just B`
*   `A and then B`
*   `B and then A`

...and ten transitions:

*   `neither` **<->** `just A`
*   `neither` **<->** `just B`
*   `just A` **<->** `A and then B`
*   `just B` **<->** `B and then A`
*   `A and then B` **-->** `just B`
*   `B and then A` **-->** `just A`

Calling `stateA.andThen(stateB)` returns an `Observable` representing the
`(A and then B)` state from above. The resulting `Observable` will only activate
on the transition between `(just A)` and `(A and then B)`, and will not activate
on the transition between `(just B)` and `(B and then A)`.

### Subscriptions as Scopes

Sometimes you might want to only `subscribe()` to an `Observable` for a limited
time, for instance, until some other `Observable` is activated. So how do you
remove an observer?

The `subscribe()` method returns a `Subscription`, which, when `close()`d, will
unregister the `Observer` registered in the `subscribe()` call. To `subscribe()`
for a limited time, simply store the `Subscription` somewhere, and call
`close()` on it when you're done.

```java
    private final Observable<String> mMessages = ...;
    private final List<String> mLog = ...;
    private Subscription mSubscription = null;

    public void startRecording() {
        if (mObserver != null) stopRecording();
        mObserver = mMessages.subscribe(Observers.onEnter(mLog::add));
    }

    public void stopRecording() {
        if (mSubscription == null) return;
        mSubscription.close();
    }
```

... wait a minute, are those `null`-checks? And a *mutable variable*? I thought
this framework was supposed to get rid of those!

And indeed we can! Since `mObserver` is a `Subscription`, which is a kind of
`Scope`, that means we can use it in another `subscribe()` call!

```java
    private final Observable<String> mMessages = ...;
    private final List<String> mLog = ...;
    private final Controller<Unit> mRecordingState = ...;

    {
        // When mRecordingState is activated, an Observer is registered to
        // mMessages.
        mRecordingState.subscribe(x -> {
            // When mRecordingState is deactivated, the Subscription is closed,
            // so new messages will stop being added to the log.
            return mMessages.subscribe(Observers.onEnter(mLog::add));
        });
    }

    public void startRecording() {
        mRecordingState.set(Unit.unit());
    }

    public void stopRecording() {
        mRecordingState.reset();
    }
```

Now we have removed the mutable variable and delegated all management of state
to `Observable`s.

But wait, we could have done the same thing with `and()`:

```java
    {
        mRecordingState.and(mMessages).subscribe(Observers.onEnter(
                (Both<Unit, String> data) -> mLog.add(data.second)));
    }
```

But here we can see the drawbacks of that approach. We need to deconstruct the
`Both` object. Though the below section shows a way to circumvent that when only
using a single `and()` call, it gets much harder to work with longer chains of
`and()`-composed `Observable`s.

Recall that deconstructing larger `Both` trees is ugly:

```java
    stateA.and(stateB).and(stateC).and(stateD).subscribe(data -> {
        A a = data.first.first.first;
        B b = data.first.first.second;
        C c = data.first.second;
        D d = data.second;
        ...
    });
```

If we only care about registering a `Scope` for when all four `Observable`s are
activated, then we can use nested `subscribe()` calls instead:

```java
    stateA.subscribe(a -> stateB.subscribe(b -> stateC.subscribe(c -> stateD.subscribe(d -> {
        ...
    }))));
```

This is called **subscription-currying**, and is a useful alternative to `and()`
calls when registering `Observer` objects for the intersection of many
`Observable`s.

To show why this works, let's simplify to just this:

```java
   stateA.subscribe(a -> stateB.subscribe(b -> ...));
```

Notice that:

* We do not `subscribe` to `stateB` until `stateA` is activated.
* If `stateB` is already activated when an `Observer` is subscribed to it, the
  observer will be notified of the data immediately.
* While `stateA` is activated, the `Observer` subscribed to `stateB` will open
  and close its scopes normally as `stateB` mutates.
* If `stateA` is deactivated, the `Subscription` to `stateB` is closed. Closing
  a `Subscription` also closes the `Scope`s from the `Observer`.

In other words, the inner `Observer` is only opened if *both* `stateA` and
`stateB` are activated, and that `Observer`'s `Scope` will be closed if *either*
`stateA` or `stateB` is deactivated. This is the same as the `and()` operator!

This even works for `Observable`s that have multiple activations. Each
activation of `stateA` will produce a unique `Subscription` to `stateB`, so this
pattern can be compared to a nested `for` loop -- one that operates reactively
whenever the states update! More precisely, there will be a 1:1 mapping of the
Cartesian product of activations of `stateA` and `stateB` and `Scopes` from the
inner `Observer` subscribed to `stateB`.

One should use the `and()` operator when more operations like `map()` and
`filter()` are needed on the resulting `Observable`, but subscription-currying
can be used in some other situations as an alternative in situations where
dealing with `Both` objects becomes cumbersome. It is generally recommended to
prefer the `and()` operator if all else is equal, because it's easier to add
more operators to the pipeline later on if needed.

An alternative to using `and()` in situations that call for `map()` and
`filter()` on the result is to use `flatMap()`-currying:

```java
  stateA.flatMap(a -> stateB.flatMap(b -> Observable.just(new C(a, b))))
      .filter(c -> isValid(c))
      .subscribe(d -> ...);
```

### Increase readability for Observers with wrapper methods

The `Observers` class contains several helper methods to increase the
fluency and readability of common cases that `Observer` objects might be
used for.

#### Use onOpen() and onClose() to observe only one kind of transition

Every `Observer` returns a `Scope`, but sometimes clients do not care about
when the state deactivates, only when it activates. It's possible to create a
`Observer` with lambda syntax to do the job like this:

```java
{
    observable.subscribe((String data) -> {
        Log.d(TAG, "activated: data=" + data);
        return () -> {};
    });
}
```

The `return () -> {};` statement in the lambda corresponds to having no
side-effects to handle the destructor, but this is not very readable.

To make intentions clearer, the `onOpen()` method can wrap any `Consumer` of
the activation data's type:

```java
{
    // Without data.
    observable.subscribe(Observer.onOpen(x -> Log.d(TAG, "activated")));
    // With data.
    observable.subscribe(Observer.onOpen((String data) -> {
        Log.d(TAG, "activated: data=" + data);
    }));
}
```

Likewise, `onClose()` is used the same way to transform any `Consumer` of the
activation data's type into a `Observer` that only has side effects when the
`Observable` is deactivated.

#### Deconstructing Both objects

When you use the `and()` method on `Observable` to create an `Observable<Both>`,
recall that the `Observer` passed to `subscribe()` must look like this:

```java
{
    observableA.and(observableB).subscribe((Both<A, B> data) -> {
        A a = data.first;
        B b = data.second;
        Log.d(TAG, "on enter: a = " + a + "; b = " + b);
        return () -> Log.d(TAG, "on exit: a = " + a + "; b = " + b);
    });
}
```

`Observers` provides a helper method to turn any function that takes two
arguments and returns a `Scope` into a `Observer<Both>`, which deconstructs
the `Both` object for you and passes the constituent parts into the function.

Using `Both.adapt()`, we can rewrite the above like this:

```java
{
    observableA.and(observableB).subscribe(Both.adapt((A a, B b) -> {
        Log.d(TAG, "on enter: a = " + a + "; b = " + b);
        return () -> Log.d(TAG, "on exit: a = " + a + "; b = " + b);
    }));
}
```

When using `onEnter()` or `onExit()`, which take `Consumer`s of the data type,
it can be useful to use `Both.adapt` on a `BiConsumer` to turn it into a
`Consumer<Both>`.

```java
{
    // Before:
    Observable<Both<A, B>> both = observableA.and(observableB);
    both.subscribe(Observers.onEnter((Both<A, B> data) -> {
        Log.d(TAG, "on enter: a = " + data.first + "; b = " + data.second);
    }));
    both.subscribe(Observers.onExit((Both<A, B> data) -> {
        Log.d(TAG, "on exit: a = " + data.first + "; b = " + data.second);
    }));
    // After:
    both.subscribe(Observers.onEnter(Both.adapt((A a, B b) -> {
        Log.d(TAG, "on enter: a = " + a + "; b = " + b);
    })));
    both.subscribe(Observers.onExit(Both.adapt((A a, B b) -> {
        Log.d(TAG, "on exit: a = " + a + "; b = " + b);
    })));
}
```

The `Both.adapt()` helpers are also able to turn `BiFunction`s into
`Function<Both>` objects and `BiPredicate`s into `Predicate<Both>` objects,
which makes them useful when using `map()` and `filter()` operators on
`Observable<Both>` objects.

```java
{
    both.map(Both.adapt((A a, B b) -> {
        return new ThingBuilder().setA(a).setB(b).build();
    }));
    both.filter(Both.adapt((A a, B b) -> a.contains(b)));
}
```

## Data flow

There are numerous instances where one may want to take the activation data of
some `Observable` and use it to set the state of a `Controller`, and reset that
`Controller` when the `Observable` is deactivated. A shortcut to doing this
without having to instantiate any `Controller` is provided with the `map()`
method in the `Observable` interface.

For example, we might have an `Activity` that overrides `onNewIntent()`, and
extracts some data from the `Intent` it receives. We might want to register
observers on the extracted data rather than the `Intent` itself, as some work
needs to be done to unparcel the data we care about from the `Intent`.

```java
public class MyActivity extends Activity {
    private final Controller<Intent> mIntentState = new Controller<>();

    {
        Observable<Uri> uriState = mIntentState.map(Intent::getData);
        Observable<String> instanceIdState = uriState.map(Uri::getPath);
        ...
    }

    public void onCreate() {
        super.onCreate();
        mIntentState.set(getIntent());
    }

    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        mIntentState.set(intent);
    }
}
```

The `map()` method takes any function on the `Observable`'s activation data and
creates a new `Observable` of the result of that function applied to the
original `Observable`'s activation data. So the activation lifetime of
`uriState` and `instanceIdState` are the same as `mIntentState` in this example.

The instance initializer can then call `subscribe()` on `uriState` or
`instanceIdState` to register callbacks for when we get a new URI or instance
ID, and the process of extracting the URI from the `Intent` and the instance ID
from the `Uri` is delegated to methods with no side-effects.

### Handling null

If a function provided to a `map()` method returns `null`, then the resulting
`Observable` will be put in a deactivated state, even if the source `Observable`
is activated. This can be used to **filter** invalid data from `Observable`s in
the pipeline:

```java
{
    mIntentState.map(Intent::getExtras)
            .map((Bundle bundle) -> bundle.getString(INTENT_EXTRA_FOO))
            .subscribe((String foo) -> ...);
}
```

The `bundle.getString()` call might return `null` if the source `Intent` does
not have the correct extra data field set. When this happens, the resulting
`Observable` simply does not activate, so the `Observer` registered in the
`subscribe()` call does not need to worry that `foo` might be `null`.

### Filtering data

One may wish to construct an `Observable` that is only activated if some
*predicate* on some other `Observable`'s activation data is true. This is easily
done using the `filter()` method on `Observable`.

This example will only log `"Got FOO intent"` if `mIntentState` was `set()` with
an `Intent` with action `"org.my.app.action.FOO"`:

```java
{
    String ACTION_FOO = "org.my.app.action.FOO";
    mIntentState.map(Intent::getAction)
            .filter(ACTION_FOO::equals)
            .subscribe(Observers.onEnter(action -> {
                Log.d(TAG, "Got FOO intent");
            }));
}
```

Since `Observable<T>#filter()` takes any `Predicate<T>`, which is a functional
interface whose method takes a `T` and returns a `boolean`, the parameter can be
an instance of a class that implements `Predicate<T>`:

```java
    class InRangePredicate implements Predicate<Integer> {
        private final int mMin;
        private final int mMax;

        private InRangePredicate(int min, int max) {
            mMin = min;
            mMax = max;
        }

        @Override
        public boolean test(Integer value) {
            return mMin <= value && value <= mMax;
        }
    }

    InRangePredicate inRange(int min, int max) {
        return new InRangePredicate(min, max);
    }

    Controller<Integer> hasIntState = new Controller<>();
    Observable<Integer> hasValidIntState = hasIntState.filter(inRange(0, 10));
```

... or a method reference for a method that takes the activation data and
returns a boolean:

```java
    class Util {
        static boolean inRange(int i) {
            return 0 <= i && i <= 10;
        }
    }
    Controller<Integer> hasIntState = new Controller<>();
    Observable<Integer> hasValidIntState = hasIntState.filter(Util::inRange);
```

... or a lambda that takes the activation data and returns a boolean:

```java
    Controller<Integer> hasIntState = new Controller<>();
    Observable<Integer> hasValidIntState =
            hasIntState.filter(i -> 0 <= i && i <= 10);
```

## Tips and best practices

### Construct the pipeline before modifying it

Consider this code:

```java
    Controller<String> c = new Controller<>();
    c.set("hi");
    c.reset();
    c.subscribe(Observers.onEnter(s -> Log.d(TAG, s)));
```

Will the callback registered in the `subscribe()` call get fired? It turns out
that it will not, since `c` is deactivated when `subscribe()` is called. But if
the `subscribe()` call is made before the `set()` call, then the callback is
fired.

Sometimes this is what you want, but it's best to avoid any ambiguity like this.
Generally, `Observable` methods like `subscribe()` should be called before any
`Controller` methods. A couple of things that one can do to help with this:

*   Instantiate `Controller` objects in field initializers, not the constructor.
*   Set up the pipeline (`subscribe()`, `and()`, `map()`, etc.) in an instance
    initializer. This is run before anything else when creating an instance,
    including the constructor, and is the same regardless of which constructor
    is being used. This also removes the potential of accidentally depending on
    constructor parameters or mutable instance variables in the pipeline, which
    can be dangerous compared to adapting them to `Observable`s.
*   In the instance initializer, `Observable`s composed from other `Observable`s
    can usually be local variables rather than instance variables. This prevents
    code outside the initializer from `subscribe()`ing these `Observable`s after
    the instance has been initialized.
*   Do not call `Controller` mutator methods (`set()` or `reset()`) inside the
    instance initializer. They may be called in the constructor or any instance
    methods.
*   Alternatively, the concerns of creating the pipeline and adapting function
    calls to state changes of `Controller`s can be separated by using a factory
    function.

### Manipulating state inside observers

What happens here?

```java
    Controller<Object> c = new Controller<>();
    c.subscribe(x -> {
        Log.d(TAG, "enter");
        c.reset();
        return () -> Log.d(TAG, "exit");
    });
    c.set("ding");
```

Here, we `reset()` the same `Controller` in an activation observer for that very
`Controller`!

This is in fact safe, though there should be few places you need to do something
like this. Currently, `Controller`s notify all observers synchronously on the
thread that `set()` or `reset()` was called in (so they are not thread safe),
but if an observer calls `set()` or `reset()` again while observers are still
being notified, the `set()` or `reset()` call gets queued and handled only after
all observers have been notified. This allows a deterministic and unastonishing
order of execution for the above example: the log will show "enter", followed
immediately by "exit".

We call this property *re-entrancy safety*. `Controller`s are *re-entrant-safe*.
What this guarantees is that all observers are notified of all changes, and any
observer that imposes its own change (including indirect changes) will not have
that change take effect until all observers are notified of the current change.

Note that if you `set()` a controller with a value that is never `null` inside
an activation handler, **you will get an infinite loop**.

```java
    Controller<Integer> c = new Controller<>();
    c.subscribe(Observers.onEnter(x -> c.set(x + 1))); // Danger!
    c.set(0); // Infinite loop!
```

Whenever the `Controller` is set with a value, the observing scope immediately
sets it with a new value, recurring infinitely.

It's possible to still be safe if you can guarantee that `set()` isn't called or
`set(null)` is called in some base case for all recursive stacks of activation
handlers, but if you do that, it's *your* job to solve the halting problem.

It is good practice to avoid calling `set()` or `reset()` on `Controller`s
inside `Observer` event handlers altogether, but there are many safe ways
that are useful.

### Testing

One of the most important aspects of using `Observable`s is that they are very
testable. Although `Observer`s themselves are not pure-functional (i.e. they
tend to mutate program state), this is done in such a way that the mutations in
the form of state transitions in `Observable`s are easy to track, and therefore
easy to test.

If you write a class that implements `Observable` or returns an `Observable` in
one of its methods, it's easy to test the events it emits by using the
`ReactiveRecorder` test utility module. This class, which is only allowed in
tests, provides a fluent interface for describing the expected output of an
`Observable`.

To use this in your tests, add `//chromecast/base:cast_base_test_utils_java` to
your JUnit test target's GN `deps`.

As an example, imagine we want to test a class called `FlipFlop`, which
implements `Observable` and changes from deactivated to activated every time its
`flip` method is called. The tests might look like this:

```java
import org.chromium.chromecast.base.ReactiveRecorder;
... // other imports
public class FlipFlopTest {
    @Test
    public void testStartsDeactivated() {
        FlipFlop f = new FlipFlop();
        ReactiveRecorder recorder = ReactiveRecorder.record(f);
        // No events should be emitted.
        recorder.verify().end();
    }

    @Test
    public void testFlipOnceActivatesObserver() {
        FlipFlop f = new FlipFlop();
        ReactiveRecorder recorder = ReactiveRecorder.record(f);
        f.flip();
        // A single activation should have been emitted.
        recorder.verify().opened(Unit.unit()).end();
    }

    @Test
    public void testFlipTwiceActivatesThenDeactivates() {
        FlipFlop f = new FlipFlop();
        ReactiveRecorder recorder = ReactiveRecorder.record(f);
        f.flip();
        f.flip();
        // Expect an activation followed by a deactivation.
        recorder.verify().opened(Unit.unit()).closed(Unit.unit()).end();
    }
}
```

The `ReactiveRecorder` class works by calling `subscribe()` on the given
`Observable` and storing the activations and deactivations it observes in a
list. The `verify()` method opens a domain-specific language for performing
assertions on the activation data, using `opened()` and `closed()` to check
which data has been activated and deactivated. The transitions recorded must
occur in the same order as the `opened()` and `closed()` calls to pass the test.
The `end()` method asserts that no more transitions occurred.

You can test behaviors that should occur when closing a `subscribe()` scope by
calling `recorder.unsubscribe()`. For example, every `Observable` implementation
should close all existing `Scopes` emitted from an `Observer` when that
`Observer`'s `subscribe()` scope is closed:

```java
    @Test
    public void testUnsubscribeCloses() {
        FlipFlop f = new FlipFlop();
        ReactiveRecorder recorder = ReactiveRecorder.record(f);
        f.flip();
        // Clear the record; we don't care about the activation from flip().
        recorder.reset();
        recorder.unsubscribe();
        // Unsubscribing should implicitly close the scope.
        recorder.verify().closed(Unit.unit()).end();
    }
```

Once a `ReactiveRecorder` unsubscribes, it will not get any new events from the
`Observable` it was recording.

### Debugging

Sometimes, particularly in long chains of `and()` or `andThen()` or `map()` or
`filter()`, you might get confused about the ultimate state of the system in
some circumstances. When confused about these complex `Observable`s, it's often
helpful to use the `debug()` operator:

```java
    // Before
    foos.andThen(bars).subscribe(...);
    // After
    foos.debug(msg -> Log.d(TAG, "foos: %s", msg))
            .andThen(bars.debug(msg -> Log.d(TAG, "bars: %s", msg)))
            .debug(msg -> Log.d(TAG, "foosAndThenBars: %s", msg))
            .subscribe(...);
```

The `debug()` operator doesn't by itself log anything; you need to give it a
`Consumer<String>` that logs the debug messages it generates. This makes the
file/line number information in the log is more helpful, as it indicates which
file you're debugging, rather than the body of the `debug()` operator itself.

The messages in question will show when `Observer`s subscribe and unsubscribe,
and whenever data are added or removed from the `Observable`. The `debug()`
operator will call the `toString()` method on all data to format messages about
state transitions.

## When to use Observables

`Observable`s and `Controller`s are intended to succinctly adapt common Android
SDK method pairs, whether they're callbacks for entering and exiting a state, or
mutators to perform state changes, into a common pattern that better separates
concerns and is composable.

### Replace mutable, nullable variables

Every mutable, nullable variable is a variable that you constantly have to
null-check before using. A `Controller` can be used to refactor these variables
into a `final` `Controller`.

The important insight is that you tend to only read a variable when state
changes, either after the variable itself is known to change, or when some other
state changes.

First, let's consider an Activity that registers a `BroadcastReceiver` in
`onStart()` and unregisters the `BroadcastReceiver` in `onStop()`.

We will ignore for now that Android tries to guarantee that pathological call
sequences like multiple `onStart()`s in a row or an `onStop()` before the first
`onStart()` will not occur, because these guarantees are not known to the Java
compiler and similar guarantees can't be relied on for all events.

```java
class MyActivity extends Activity {
    private BroadcastReceiver mReceiver = null;

    @Override
    public void onStart() {
        super.onStart();
        if (mReceiver != null)
            unregisterReceiver(mReceiver);
        mReceiver = new BroadcastReceiver(...);
        registerReceiver(mReceiver);
    }

    @Override
    public void onStop() {
        if (mReceiver != null)
            unregisterReceiver(mReceiver);
        mReceiver = null;
        super.onStop();
    }
}
```

Without the assumption that Android will call `onStart()` and `onStop()` in
reasonable orders, we need to check the state of the `mReceiver` variable each
time before it is used. And making that assumption is prone to backfiring, as
it's a recipe for `NullPointerException`s, `IllegalStateException`s, and other
horrors in general practice.

Here's the refactored version that uses a `Controller`:

```java
class MyActivity extends Activity {
    private final Controller<Unit> mStartedState = new Controller<>();

    {
        mStartedState.subscribe(x -> {
            final BroadcastReceiver receiver = new BroadcastReceiver(...);
            registerReceiver(receiver);
            return () -> unregisterReceiver(receiver);
        });
    }

    @Override
    public void onStart() {
        super.onStart();
        mStartedState.set(Unit.unit());
    }

    @Override
    public void onStop() {
        mStartedState.reset();
        super.onStop();
    }
}
```

The refactored version better separates concerns. `BroadcastReceiver`
registration and unregistration is handled in a small area of the code, rather
than spread throughout the class, and the `BroadcastReceiver` doesn't need to be
stored in a mutable variable. *No code outside the scope in which the
`BroadcastReceiver` object is relevant can touch it*, and the `onStart()` and
`onStop()` methods have no logic except toggling the `Controller` that
represents whether the `Activity` is started. Best of all, there are no `null`
checks, and no need for any.

### Asynchronous initialization

Some methods run asynchronously and take a callback that is run when the work is
complete. We can `set()` `Controller`s in such callbacks to adapt this pattern
to `Observable`s, which can be used to create asynchronous initialization
pipelines.

This example shows how one can link up the outputs of multiple asynchronous
functions that use the callback-passing style using `Controller`s, and
encapsulating the complicated setup into a single function that returns an
`Observable`.

```java
public class AsyncExample {
    private static final String TAG = "AsyncExample";

    // Adapts the callback-style asynchronous Baz function to an Observable.
    // Shows how a
    public static Observable<Baz> createBazDefault() {
        // Begin constructing a Foo.
        Controller<Foo> fooState = new Controller<>();
        // Set the fooState controller when created, reset on errors.
        Foo.createAsync(fooState::set, fooState::reset);
        // Bar requires Foo to initialize.
        Controller<Bar> barState = new Controller<>();
        fooState.subscribe((Foo foo) -> {
            Bar.createAsync(foo, barState::set);
            // If fooState is reset, then barState is also reset.
            return barState::reset;
        });
        // Baz requires Foo and Bar to initialize.
        Controller<Baz> bazState = new Controller<>();
        fooState.and(barState).subscribe(Observers.both((Foo foo, Bar bar) -> {
            Baz.createAsync(foo, bar, bazState::set);
            // If fooState or barState is reset, then bazState is also reset.
            return bazState::reset;
        }));
        return bazState;
    }

    public static void demo() {
        Observable<Baz> bazState = createBazDefault();
        bazState.subscribe(Observers.onEnter((Baz baz) -> {
            // This runs when the full initialization pipeline is complete.
            Log.d(TAG, "Baz created!");
        }));
    }

    public static class Foo {
        static void createAsync(Consumer<Foo> callback, Runnable onError) {...}
    }

    public static class Bar {
        static void createAsync(Foo foo, Consumer<Bar> callback) {...}
    }

    public static class Baz {
        static void createAsync(Foo foo, Bar bar, Consumer<Baz> callback) {...}
    }
}
```

This way, `Observable`s can be used similarly to `Promise`s, where a callback
handling the underlying value can be registered before the underlying value is
available. But unlike `Promise`s, `Observable`s provide a way to also handle
teardowns, and to transitively tear down everything down stream when something
is torn down.
