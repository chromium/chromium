// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Safe Observer/Observable implementation.
//
// When using ObserverListThreadSafe, we were running into issues since there
// was no synchronization between getting the existing value, and registering
// as an observer. See go/cast-platform-design-synchronized-value for more
// details.
//
// To fix this issue, and to make observing values safer and simpler in general,
// use the Observable/Observer pattern in this file. When you have a value that
// other code wants to observe (ie, get the value of and receive any changes),
// wrap that value in an Observable<T>. The value type T must be copyable and
// copy-assignable. The Observable must be constructed with the initial observed
// value, and the value may be updated at any time from any thread by calling
// SetValue(). You can also get the value using GetValue(); but note that this
// is not threadsafe (the value is returned without locking), so the caller must
// ensure safety by other means. Calling GetValueThreadSafe() is threadsafe but
// involves a mutex lock.
//
// Code that wants to observe the value calls Observe() on it at any point when
// the value is alive. Note that Observe() may be called safely from any thread.
// Observe() returns an Observer<T> instance, which MUST be used and destroyed
// only on the thread that called Observe(). The Observer initially contains the
// value that the Observable had when Observe() was called, and that value will
// be updated asynchronously whenever the Observable's SetValue() method is
// is called. NOTE: the initial value of the Observer is the value known to the
// thread that created the Observer at the time; there may be an updated value
// from the Observable that hasn't been handled by the Observer's thread yet.
//
// The Observer's view of the observed value is returned by GetValue(); this is
// a low-cost call since there is no locking (the value is updated on the thread
// that constructed the Observer). Note that Observers are always updated
// asynchronously with PostTask(), even if they belong to the same thread that
// calls SetValue(). All Observers on the same thread have the same consistent
// view of the observed value.
//
// Observers may be copied freely; the copy also observes the original
// Observable, and belongs to the thread that created the copy. Copying is safe
// even when the original Observable has been destroyed.
//
// Code may register a callback that is called whenever an Observer's value is
// updated, by calling SetOnUpdateCallback(). If you get an Observer by calling
// Observe() and then immediately call SetOnUpdateCallback() to register a
// a callback, you are guaranteed to get every value of the Observable starting
// from when you called Observe() - you get the initial value by calling
// GetValue() on the returned Observer, and any subsequent updates will trigger
// the callback so you can call GetValue() to get the new value. You will not
// receive any extra callbacks (exactly one callback per value update).
//
// Note that Observers are not default-constructible, since there is no way to
// construct it in a default state. In cases where you need to instantiate an
// Observer after your constructor, you can use a std::unique_ptr<Observer>
// instead, and initialize it when needed.
//
// Example usage:
//
// class MediaManager {
//  public:
//   MediaManager() : volume_(0.0f) {}
//
//   Observer<float> ObserveVolume() { return volume_.Observe(); }
//
//   // ... other methods ...
//
//  private:
//   // Assume this is called from some other internal code when the volume is
//   // updated.
//   void OnUpdateVolume(float new_volume) {
//     volume_.SetValue(new_volume);  // All observers will get the new value.
//   }
//
//   Observable<float> volume_;
// }
//
// class VolumeFeedbackManager {
//  public:
//   VolumeFeedbackManager(MediaManager* media_manager)
//       : volume_observer_(media_manager->ObserveVolume()) {
//     volume_observer_.SetOnUpdateCallback(
//         base::BindRepeating(&VolumeFeedbackManager::OnVolumeChanged,
//                             base::Unretained(this)));
//   }
//
//  private:
//   void OnVolumeChanged() {
//     ShowVolumeFeedback(volume_observer_.GetValue());
//   }
//
//   void ShowVolumeFeedback(float volume) {
//     // ... some implementation ...
//   }
// };
//

#ifndef CHROMECAST_BASE_OBSERVER_H_
#define CHROMECAST_BASE_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromecast {

namespace subtle {
template <typename T>
class ObservableInternals;
}  // namespace subtle

template <typename T>
class Observable;

template <typename T>
class Observer {
 public:
  Observer(const Observer& other);

  ~Observer();

  void SetOnUpdateCallback(base::RepeatingClosure callback) {
    on_update_callback_ = std::move(callback);
  }

  const T& GetValue() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return value_;
  }

 private:
  friend class subtle::ObservableInternals<T>;
  friend class Observable<T>;

  explicit Observer(scoped_refptr<subtle::ObservableInternals<T>> internals);

  void OnUpdate();

  const scoped_refptr<subtle::ObservableInternals<T>> internals_;
  // Note: value_ is a const ref to the value copy for this sequence, stored in
  // SequenceOwnedInfo.
  const T& value_;
  base::RepeatingClosure on_update_callback_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_ASSIGN(Observer);
};

template <typename T>
class Observable {
  static_assert(std::is_copy_constructible<T>::value,
                "Observable values must be copyable");
  static_assert(std::is_copy_assignable<T>::value,
                "Observable values must be copy-assignable");

 public:
  explicit Observable(const T& initial_value);
  Observer<T> Observe();

  void SetValue(const T& new_value);
  const T& GetValue() const;  // NOT threadsafe!
  T GetValueThreadSafe() const;

 private:
  // By using a refcounted object to store the value and observer list, we can
  // avoid tying the lifetime of Observable to its Observers or vice versa.
  const scoped_refptr<subtle::ObservableInternals<T>> internals_;

  DISALLOW_COPY_AND_ASSIGN(Observable);
};

namespace subtle {

template <typename T>
class ObservableInternals
    : public base::RefCountedThreadSafe<ObservableInternals<T>> {
 public:
  explicit ObservableInternals(const T& initial_value)
      : value_(initial_value) {}

  void SetValue(const T& new_value) {
    base::AutoLock lock(lock_);
    value_ = new_value;

    for (auto& item : per_sequence_) {
      item.SetValue(new_value);
    }
  }

  const T& GetValue() const { return value_; }

  T GetValueThreadSafe() const {
    base::AutoLock lock(lock_);
    return value_;
  }

  const T& AddObserver(Observer<T>* observer) {
    DCHECK(observer);
    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    auto task_runner = base::SequencedTaskRunnerHandle::Get();

    base::AutoLock lock(lock_);
    auto it = per_sequence_.begin();
    while (it != per_sequence_.end() && it->task_runner() != task_runner) {
      ++it;
    }
    if (it == per_sequence_.end()) {
      per_sequence_.emplace_back(std::move(task_runner), value_);
      it = --per_sequence_.end();
    }
    it->AddObserver(observer);
    return it->value();
  }

  void RemoveObserver(Observer<T>* observer) {
    DCHECK(observer);
    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    auto task_runner = base::SequencedTaskRunnerHandle::Get();

    base::AutoLock lock(lock_);
    for (size_t i = 0; i < per_sequence_.size(); ++i) {
      if (per_sequence_[i].task_runner() == task_runner) {
        per_sequence_[i].RemoveObserver(observer);

        if (per_sequence_[i].Empty()) {
          per_sequence_[i].Swap(per_sequence_.back());
          per_sequence_.pop_back();
        }
        return;
      }
    }

    NOTREACHED() << "Tried to remove observer from unknown task runner";
  }

 private:
  // Information owned by a particular sequence. Must be only accessed on that
  // sequence, and must be deleted by posting a task to that sequence.
  // This class MUST NOT contain a scoped_refptr to the task_runner, since if it
  // did, there would be a reference cycle during cleanup, when the task to
  // Destroy() is posted.
  class SequenceOwnedInfo {
   public:
    explicit SequenceOwnedInfo(const T& value) : value_(value) {}

    const T& value() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return value_;
    }

    void AddObserver(Observer<T>* observer) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      DCHECK(observer);
      DCHECK(!base::Contains(observers_, observer));
      observers_.push_back(observer);
    }

    void RemoveObserver(Observer<T>* observer) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      DCHECK(observer);
      DCHECK(base::Contains(observers_, observer));
      observers_.erase(
          std::remove(observers_.begin(), observers_.end(), observer),
          observers_.end());
    }

    bool Empty() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return observers_.empty();
    }

    void SetValue(const T& value) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      value_ = value;
      for (auto* obs : observers_) {
        obs->OnUpdate();
      }
    }

    static void Destroy(std::unique_ptr<SequenceOwnedInfo> self) {
      // The unique_ptr deletes automatically.
    }

   private:
    std::vector<Observer<T>*> observers_;
    T value_;
    SEQUENCE_CHECKER(sequence_checker_);

    DISALLOW_COPY_AND_ASSIGN(SequenceOwnedInfo);
  };

  class PerSequenceInfo {
   public:
    PerSequenceInfo(scoped_refptr<base::SequencedTaskRunner> task_runner,
                    const T& value)
        : task_runner_(std::move(task_runner)),
          owned_info_(std::make_unique<SequenceOwnedInfo>(value)) {}

    PerSequenceInfo(PerSequenceInfo&& other) = default;

    ~PerSequenceInfo() {
      if (!owned_info_) {
        // Members have been moved out via move constructor.
        return;
      }

      DCHECK(Empty());
      // Must post a task to delete the owned info, since there may still be a
      // pending task to call SequenceOwnedInfo::SetValue().
      // Use manual PostNonNestableTask(), since DeleteSoon() does not
      // guarantee deletion.
      task_runner_->PostNonNestableTask(
          FROM_HERE,
          base::BindOnce(&SequenceOwnedInfo::Destroy, std::move(owned_info_)));
    }

    const T& value() const { return owned_info_->value(); }

    const base::SequencedTaskRunner* task_runner() const {
      return task_runner_.get();
    }

    void AddObserver(Observer<T>* observer) {
      owned_info_->AddObserver(observer);
    }

    void RemoveObserver(Observer<T>* observer) {
      owned_info_->RemoveObserver(observer);
    }

    bool Empty() const { return owned_info_->Empty(); }

    void Swap(PerSequenceInfo& other) {
      std::swap(task_runner_, other.task_runner_);
      std::swap(owned_info_, other.owned_info_);
    }

    void SetValue(const T& value) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&SequenceOwnedInfo::SetValue,
                         base::Unretained(owned_info_.get()), value));
    }

   private:
    // Operations on |owned_info| do not need to be synchronized with a lock,
    // since all operations must occur on |task_runner|.
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    std::unique_ptr<SequenceOwnedInfo> owned_info_;
  };

  friend class base::RefCountedThreadSafe<ObservableInternals>;
  ~ObservableInternals() {}

  mutable base::Lock lock_;
  T value_;
  std::vector<PerSequenceInfo> per_sequence_;

  DISALLOW_COPY_AND_ASSIGN(ObservableInternals);
};

}  // namespace subtle

template <typename T>
Observer<T>::Observer(scoped_refptr<subtle::ObservableInternals<T>> internals)
    : internals_(std::move(internals)), value_(internals_->AddObserver(this)) {}

template <typename T>
Observer<T>::Observer(const Observer& other) : Observer(other.internals_) {}

template <typename T>
Observer<T>::~Observer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  internals_->RemoveObserver(this);
}

template <typename T>
void Observer<T>::OnUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_update_callback_) {
    on_update_callback_.Run();
  }
}

template <typename T>
Observable<T>::Observable(const T& initial_value)
    : internals_(base::WrapRefCounted(
          new subtle::ObservableInternals<T>(initial_value))) {}

template <typename T>
Observer<T> Observable<T>::Observe() {
  return Observer<T>(internals_);
}

template <typename T>
void Observable<T>::SetValue(const T& new_value) {
  internals_->SetValue(new_value);
}

template <typename T>
const T& Observable<T>::GetValue() const {
  return internals_->GetValue();
}

template <typename T>
T Observable<T>::GetValueThreadSafe() const {
  return internals_->GetValueThreadSafe();
}

}  // namespace chromecast

#endif  // CHROMECAST_BASE_OBSERVER_H_
