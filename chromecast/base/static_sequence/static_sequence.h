// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_STATIC_SEQUENCE_STATIC_SEQUENCE_H_
#define CHROMECAST_BASE_STATIC_SEQUENCE_STATIC_SEQUENCE_H_

#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"

// Allows sequences to be defined at compile time so that objects can opt into
// requiring that their methods are called on a specific sequence in a way that
// can be checked by the compiler rather than DCHECKs.
//
// To define a sequence, just create a class that extends this one using the
// Curiously Recurring Template Pattern:
//
//   struct MySequence : util::StaticSequence<MySequence> {};
//
// To require that a function run on that sequence, add a Key parameter from the
// sequence:
//
//   void MyFunction(int x, int y, const MySequence::Key&);
//
// Such a function must be called through the MySequence's PostTask() method:
//
//   // Can run on any thread.
//   void MyFunctionThreadSafe(int x, int y) {
//     MySequence::PostTask(FROM_HERE, base::BindOnce(&MyFunction, x, y));
//   }
//
// You can also add the Key as the final parameter to instance methods to
// similarly require that the method be called on the sequence:
//
//   struct MyStruct {
//     // The Key needs to be the last parameter!
//     void MyMethod(int x, int y, const MySequence::Key&);
//   };
//
//   void CallMyMethodFromOriginThreadSafe(MyStruct* m) {
//     MySequence::PostTask(
//         FROM_HERE,
//         base::BindOnce(&MyStruct::MyMethod, base::Unretained(m), 0, 0));
//   }
//
// If a class is tightly coupled to a given sequence (i.e. expects to always be
// called on that sequence), it may be worth wrapping in Sequenced, which is
// similar to base::SequenceBound but will work with statically-sequenced
// method calls. This will also ensure the destructor is run on the same
// sequence.

namespace util {

template <typename T, typename TraitsProvider>
class StaticSequence;

namespace internal {

// Provides a TaskRunner and can persist after the message loop is destroyed,
// which is useful if e.g. a StaticTaskRunnerHolder outlives a
// base::test::TaskEnvironment in tests. Only usable by StaticSequence.
class StaticTaskRunnerHolder
    : public base::MessageLoopCurrent::DestructionObserver {
 public:
  ~StaticTaskRunnerHolder() override;

 private:
  template <typename T, typename TraitsProvider>
  friend class ::util::StaticSequence;

  explicit StaticTaskRunnerHolder(base::TaskTraits traits);

  void WillDestroyCurrentMessageLoop() override;

  const scoped_refptr<base::SequencedTaskRunner>& Get();

  const base::TaskTraits traits_;
  bool initialized_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace internal

// Default traits for a static sequence. They can be overridden by specifying
// another struct with a GetTraits() static method as the second template
// parameter to StaticSequence.
//
// Example:
//
//   class MyBackgroundService {
//     struct BackgroundTaskTraitsProvider {
//       static constexpr base::TaskTraits GetTraits() {
//         return {
//           base::ThreadPool(),
//           base::TaskPriority::BEST_EFFORT,
//           base::MayBlock(),
//         };
//       }
//     };
//    public:
//     struct BackgroundSequence
//         : util::StaticSequence<BackgroundSequence,
//                                BackgroundTaskTraitsProvider> {};
//     void DoBackgroundWork(const std::string& request,
//                           const BackgroundSequence::Key&);
//   };
struct DefaultStaticSequenceTraitsProvider {
  static constexpr base::TaskTraits GetTraits() { return {base::ThreadPool()}; }
};

// A class that extends StaticSequence is a holder for a process-global
// TaskRunner that is created on-demand with the desired traits, which also
// provides static PostTask overloads that can take callbacks that require a
// special Key that only the StaticSequence can provide. This trick is what
// guarantees at compile time that all invocations of a statically-sequenced
// function are run on the correct TaskRunner.
template <typename T,
          typename TraitsProvider = DefaultStaticSequenceTraitsProvider>
class StaticSequence {
 public:
  // Can only be constructed by the StaticSequence implementation. This
  // restriction allows functions and methods to statically assert that they are
  // being called on the correct sequence because StaticSequences will only
  // provide a reference to its Key through their PostTask() method.
  //
  // The reference can be passed around, but the key itself cannot be copied or
  // moved, and the address cannot be taken.
  class Key {
   public:
    using Sequence = T;
    ~Key() = default;

   private:
    friend class StaticSequence;
    constexpr Key() = default;
    // Cannot copy, move, or take the address of a Key. This prevents the common
    // ways one might attempt to obtain a Key outside the scope where it is
    // valid.
    Key(const Key&) = delete;
    Key& operator=(const Key&) = delete;
    const Key* operator&() const = delete;
  };

  static const scoped_refptr<base::SequencedTaskRunner>& TaskRunner() {
    // A StaticTaskRunnerHolder is able to regenerate a TaskRunner after the
    // global thread pool is destroyed and re-created (which can happen between
    // unittests that use base::test::TaskEnvironment).
    static internal::StaticTaskRunnerHolder task_runner(
        TraitsProvider::GetTraits());
    return task_runner.Get();
  }

  // Catches you if you attempt to post a callback that consumes a Key of
  // another StaticSequence. The compiler will print a message containing
  // PostedTo, the StaticSequence whose PostTask method was called; and
  // Expected, the StaticSequence whose Key was requested by the task.
  template <typename U>
  using IncompatibleCallback = base::OnceCallback<void(const U&)>;
  template <typename U, typename Expected = typename U::Sequence>
  static void PostTask(
      IncompatibleCallback<U> cb,
      const base::Location& from_here = base::Location::Current()) {
    using PostedTo = T;
    static_assert(invalid<PostedTo, Expected>,
                  "Attempting to post a statically-sequenced task to the wrong "
                  "static sequence!");
  }

  template <typename U>
  using IncompatibleNonConstCallback = base::OnceCallback<void(U&)>;
  template <typename U, typename Expected = typename U::Sequence>
  static void PostTask(
      IncompatibleNonConstCallback<U> cb,
      const base::Location& from_here = base::Location::Current()) {
    static_assert(invalid<IncompatibleNonConstCallback<U>>,
                  "Did you forget to add `const` to the Key parameter of the "
                  "bound functor?");
  }

  // Takes a callback that specifically requires that it be invoked from this
  // sequence. Such callbacks can only be invoked through this method because
  // the Key is only constructible here.
  using CompatibleCallback = base::OnceCallback<void(const Key&)>;
  static void PostTask(
      CompatibleCallback cb,
      const base::Location& from_here = base::Location::Current()) {
    TaskRunner()->PostTask(from_here,
                           base::BindOnce(std::move(cb), std::ref(key_)));
  }

  // Takes any closure with no unbound arguments.
  static void PostTask(
      base::OnceClosure cb,
      const base::Location& from_here = base::Location::Current()) {
    TaskRunner()->PostTask(from_here, std::move(cb));
  }

  // The Run() overload set can only be invoked on the sequence, and accepts
  // callbacks that may or may not require a Key to the sequence.
  static void Run(CompatibleCallback cb, const Key& key) {
    std::move(cb).Run(key);
  }
  static void Run(base::OnceClosure cb, const Key&) { std::move(cb).Run(); }
  template <typename U, typename Expected = typename U::Sequence>
  static void Run(IncompatibleCallback<U> cb, const Key&) {
    using PostedTo = T;
    static_assert(invalid<PostedTo, Expected>,
                  "Attempting to post a statically-sequenced task to the wrong "
                  "static sequence!");
  }
  template <typename U>
  static void Run(IncompatibleNonConstCallback<U> cb, const Key&) {
    static_assert(invalid<IncompatibleNonConstCallback<U>>,
                  "Did you forget to add `const` to the Key parameter of the "
                  "bound functor?");
  }

  // Forwards a functor and arguments before posting as a task, to avoid
  // unnecessary mallocs. Prefer this to PostTask() when possible to reduce
  // runtime overhead.
  template <typename F, typename... Args>
  static void Post(const base::Location& from_here, F&& f, Args&&... args) {
    TaskRunner()->PostTask(
        from_here, BindHelper<needs_key<F>, F, Args...>::Bind(
                       std::forward<F>(f), std::forward<Args>(args)...));
  }

 private:
  // Used to help print readable compiler messages in static_assert failures.
  template <typename... Args>
  constexpr static bool invalid = false;

  template <typename... Ts>
  struct Pack;

  template <typename Pack>
  struct LastArgumentIsKey;

  template <typename First, typename... Rest>
  struct LastArgumentIsKey<Pack<First, Rest...>>
      : LastArgumentIsKey<Pack<Rest...>> {};

  template <>
  struct LastArgumentIsKey<Pack<const Key&>> : std::true_type {};

  template <>
  struct LastArgumentIsKey<Pack<>> : std::false_type {};

  template <typename F>
  struct GetArgs;

  template <typename R, typename... Args>
  struct GetArgs<R (*)(Args...)> {
    using type = Pack<Args...>;
  };

  template <typename R, typename Obj, typename... Args>
  struct GetArgs<R (Obj::*)(Args...)> {
    using type = Pack<Args...>;
  };

  template <typename F>
  constexpr static bool needs_key =
      LastArgumentIsKey<typename GetArgs<F>::type>::value;

  template <bool requires_key, typename... Args>
  struct BindHelper;

  template <typename... Args>
  struct BindHelper<false, Args...> {
    static base::OnceClosure Bind(Args... args) {
      return base::BindOnce(std::forward<Args>(args)...);
    }
  };

  template <typename... Args>
  struct BindHelper<true, Args...> {
    static base::OnceClosure Bind(Args... args) {
      return base::BindOnce(std::forward<Args>(args)..., std::ref(key_));
    }
  };

  static const Key key_;
};

template <typename T, typename TraitsProvider>
const typename StaticSequence<T, TraitsProvider>::Key
    StaticSequence<T, TraitsProvider>::key_ = {};

// Behaves like the SequenceBound class wrapper for static sequences, wrapping
// an object and forcing all method calls to go through Post(), which ensures
// they are all called on the statically assigned sequence, whether the methods
// ask for a Key or not.
template <typename T, typename Sequence>
class Sequenced {
 public:
  template <typename... Args>
  explicit Sequenced(Args&&... args) : obj_(Uninitialized()) {
    Sequence::Post(FROM_HERE, &Sequenced::Construct<Args...>,
                   base::Unretained(this), std::forward<Args>(args)...);
  }

  template <typename... Args, typename... Bound>
  void Post(const base::Location& from_here,
            void (T::*method)(Args...),
            Bound&&... args) {
    Sequence::Post(from_here, &Sequenced::Call<decltype(method), Bound...>,
                   base::Unretained(this), method,
                   std::forward<Bound>(args)...);
  }

 private:
  using UniquePtr = std::unique_ptr<T, base::OnTaskRunnerDeleter>;
  template <typename... Args>
  void Construct(Args&&... args, const typename Sequence::Key& key) {
    obj_ = MakeUnique<Args...>(std::forward<Args>(args)..., key);
  }

  static UniquePtr Uninitialized() {
    return UniquePtr(nullptr,
                     base::OnTaskRunnerDeleter(Sequence::TaskRunner()));
  }

  template <typename... Args>
  UniquePtr MakeUnique(Args&&... args, const typename Sequence::Key&) {
    return UniquePtr(new T(std::forward<Args>(args)...),
                     base::OnTaskRunnerDeleter(Sequence::TaskRunner()));
  }

  template <typename Method, typename... Bound>
  void Call(Method method, Bound&&... args, const typename Sequence::Key& key) {
    Sequence::Run(base::BindOnce(method, base::Unretained(obj_.get()),
                                 std::forward<Bound>(args)...),
                  key);
  }

  UniquePtr obj_;
};

}  // namespace util

#endif  // CHROMECAST_BASE_STATIC_SEQUENCE_STATIC_SEQUENCE_H_
