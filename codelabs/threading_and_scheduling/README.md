# Threading & scheduling

This directory contains three examples of threading & primitives in Chromium's
`//base` library that build off of the basic `cpp101` [threading and task runner
introduction][1]. In particular, these examples aim to give more insight into
how task scheduling works by introducing `TaskQueue` as the basic unit of
scheduling, and showing how multiple `TaskQueue`s can interact within the same
thread and across threads.

Additionally, these examples show direct usage of some of the foundational
scheduling primitives in `//base` as they would be used in any program, to
demystify their usage and help illustrate how core pieces of Chromium work under
the hood.

These examples offer a deep dive into the threading & scheduling machinery that
Chromium developers will rarely interact with directly. In practice, much of
Chromium's threading & scheduling is exposed through things like
`base::ThreadPool`, `base::SequencedTaskRunner::GetCurrentDefault()`, or
`ExecutionContext::GetTaskRunner()` in Blink, etc.

## 01-single-task-queue

This first example is the simplest, but illustrates direct usage of the
following task scheduling primitives:

 - `SequenceManager(Impl)`
 - `MessagePump`
 - `TaskQueue` / `TaskRunner`
 - `RunLoop`

This example goes further than the ones in `cpp101/`, as those defer the setup
of the main thread scheduling infra to `base::test::TaskEnvironment`, whereas
this example shows how to manually use such infrastructure and customize it
accordingly.

## 02-task-queue-priorities

This example shows how `TaskQueue`s allow a thread to multiplex many sources of
tasks. This effectively means that each `TaskQueue` is a "sequence", allowing
the same physical thread to host multiple sequences that can be scheduled
independently. This is an inversion from the standard usage of "sequence", which
often involves a sequential ordering of tasks that may opaquely span multiple
physical threads.

This example works by creating two `TaskQueue`s with different priorities to
show that the ordering _across_ queues is not guaranteed, while the ordering of
non-delayed tasks _within_ a queue — regardless of which `TaskRunner` is
used to post the task — is always FIFO order.

## 03-randomized-task-queues

(Requires DCHECKs to be enabled to build. Either set `is_debug = true` or
`dcheck_always_enabled = true` in your gn args).

This example is similar to the previous one, however we schedule two
identical-priority `TaskQueue`s independently from each other using a
[randomized task selection seed option][2] exposed via the `SequenceManager`
API. This shows that in our contrived example, ordering _across_ queues of the
same priority is not guaranteed to be in FIFO task posting order, while the
ordering of non-delayed tasks _within_ the same queue is _still_ always FIFO
order.

Note that in practice and by default, the ordering of tasks _across_ task queues
with the same priority _is_ in fact guaranteed to be in FIFO posting-order
because legacy code has come to depend on this behavior, although it should not
be relied upon, as queues can dynamically [change priority][3] and get
[frozen][4]/unfrozen arbitrarily.

## 04-multiple-threads

So far the threading and task posting examples have been either single-threaded,
or utilize the thread pool. But in Chromium, especially in renderer processes,
it's common to post tasks on specific threads that the platform maintains.
That's exactly what the scenario that this example reproduces. First we setup
the main thread task scheduling infra, and then spin up a new `base::Thread`
which represents a new physical thread. Then we explicitly post tasks back and
forth between the main and secondary thread, via a cross-thread `TaskRunner`.

[1]: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/codelabs/cpp101/README.md#part-3_threads-and-task-runners
[2]: https://source.chromium.org/chromium/chromium/src/+/main:base/task/sequence_manager/sequence_manager.h;l=122-125;drc=c70927109b0861ba4642416cb4689b4bf9d25ad0
[3]: https://source.chromium.org/chromium/chromium/src/+/main:base/task/sequence_manager/task_queue.cc;l=246;drc=566a5d280c3f78e073c435a5218021ce15d1f004
[4]: https://source.chromium.org/chromium/chromium/src/+/main:base/task/sequence_manager/task_queue_impl.cc;l=1132;drc=74e2db96056dd0265b7e527a58b50036c07d7031
