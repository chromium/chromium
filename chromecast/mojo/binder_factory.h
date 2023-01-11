// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MOJO_BINDER_FACTORY_H_
#define CHROMECAST_MOJO_BINDER_FACTORY_H_

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromecast {

// A class to provide callback for binding mojo PendingReceiver with
// mojo interface <T> implementation. This class is designed to be inherited by
// mojo interface implementation. Service class can call BinderFactory class's
// GetBinder() directly to add the binder callback to a BinderRegistry.
// Binding management on interface implementation side can be avoided by
// utilizing this.
//
// For example,
//
// demo_code_impl.h:
//
//   class DemoCodeImpl : public mojom::DemoCode,
//                        public BinderFactory<mojom::DemoCode> {
//    public:
//     DemoCodeImpl();
//     ...
//    private:
//     ...
//   };
//
// demo_service.h:
//
//   class DemoService : public service_manager::Service {
//    public:
//     DemoService();
//     ~DemoService();
//
//     // service_manager::Service implementation:
//     void OnStart() override;
//     void OnBindInterface(const service_manager::BindSourceInfo& source,
//                          const std::string& interface_name,
//                          mojo::ScopedMessagePipeHandle interface_pipe)
//                          override;
//    private:
//     ...
//     service_manager::BinderRegistry registry_;
//     std::unique_ptr<DemoCodeImpl> demo_code_;
//     ...
//   }
//
// demo_service.cc:
//
//   void DemoService::OnStart() {
//     registry_.AddInterface<mojom::DemoCode>(demo_code_.GetBinder());
//   }
//
//   void DemoService::OnBindInterface(
//       const service_manager::BindSourceInfo& source,
//       const std::string& interface_name,
//       mojo::ScopedMessagePipeHandle interface_pipe) {
//     registry_.TryBindInterface(interface_name, &interface_pipe);
//   }

// Non-template base class for BinderFactory. This allows multiple instances of
// BinderFactoryBase to be stored in a generic data structure.
class BinderFactoryBase {
 public:
  virtual ~BinderFactoryBase() {}

  // Binds a message pipe handle to an endpoint.
  virtual void BindPipe(mojo::ScopedMessagePipeHandle handle) = 0;

  // Returns a callback which wraps BinderFactoryBase::BindPipe().
  virtual base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>
  GetPipeBinder() = 0;
};

template <typename Interface>
class BinderFactory : public BinderFactoryBase {
 public:
  explicit BinderFactory(
      Interface* impl,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr,
      bool single_binder = false)
      : impl_(impl),
        task_runner_(std::move(task_runner)),
        use_single_binder_(single_binder),
        single_receiver_(impl_),
        weak_factory_(this) {
    DCHECK(impl_) << "Implementation for interface '" << Interface::Name_
                  << "' is null!";
  }
  BinderFactory(const BinderFactory&) = delete;

  ~BinderFactory() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  BinderFactory& operator=(const BinderFactory&) = delete;

  base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)> GetBinder() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::BindRepeating(&BinderFactory::Bind,
                               weak_factory_.GetWeakPtr());
  }

  void Bind(mojo::PendingReceiver<Interface> request) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (use_single_binder_) {
      if (single_receiver_.is_bound()) {
        LOG(ERROR) << Interface::Name_ << " implementation is already bound, "
                   << " this request will be dropped.";
        return;
      }
      single_receiver_.Bind(std::move(request), task_runner_);
      single_receiver_.set_disconnect_handler(
          base::BindRepeating(&mojo::Receiver<Interface>::reset,
                              base::Unretained(&single_receiver_)));
    } else {
      receivers_.Add(impl_, std::move(request), task_runner_);
    }
  }

  // BinderFactoryBase implementation:
  base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> GetPipeBinder()
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::BindRepeating(&BinderFactory::BindPipe,
                               weak_factory_.GetWeakPtr());
  }

  void BindPipe(mojo::ScopedMessagePipeHandle handle) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    receivers_.Add(impl_, mojo::PendingReceiver<Interface>(std::move(handle)),
                   task_runner_);
  }

 private:
  Interface* const impl_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const bool use_single_binder_;

  mojo::ReceiverSet<Interface> receivers_;
  mojo::Receiver<Interface> single_receiver_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BinderFactory> weak_factory_;
};

// Syntactic sugar for classes that want to implement a single <Interface> and
// also receive the helpful binding functions for BinderFactory.
template <typename Interface>
class Bindable : public BinderFactory<Interface>, public Interface {
 public:
  Bindable() : BinderFactory<Interface>(this) {}
  ~Bindable() override = default;
};

// This implementation wraps a callback that accepts a PendingReceiver. This
// allows MultiBinderFactory to work with clients that provide their own binding
// logic.
template <typename Interface>
class BinderCallbackWrapper : public BinderFactoryBase {
 public:
  explicit BinderCallbackWrapper(
      const base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>&
          callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
      : callback_(callback), task_runner_(task_runner), weak_factory_(this) {
    DCHECK(callback_);
  }
  BinderCallbackWrapper(const BinderCallbackWrapper&) = delete;
  BinderCallbackWrapper& operator=(const BinderCallbackWrapper&) = delete;

  // BinderFactoryBase implementation:
  base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> GetPipeBinder()
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::BindRepeating(&BinderCallbackWrapper::BindPipe,
                               weak_factory_.GetWeakPtr());
  }

  void BindPipe(mojo::ScopedMessagePipeHandle handle) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mojo::PendingReceiver<Interface> request(std::move(handle));
    if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&BinderCallbackWrapper::BindOnTaskRunner,
                         weak_factory_.GetWeakPtr(), std::move(request)));
      return;
    }
    callback_.Run(std::move(request));
  }

 private:
  void BindOnTaskRunner(mojo::PendingReceiver<Interface> request) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    callback_.Run(std::move(request));
  }

  base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)> callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BinderCallbackWrapper> weak_factory_;
};

// Similar to BinderFactory, MultiBinderFactory can provide binder functions for
// more than one interface.
//
// Example code:
//
//   MultiBinderFactory factory;
//   factory.AddInterface<mojom::Foo>(&foo_impl);
//   factory.AddInterface<mojom::Bar>(&bar_impl);
//
//   service_manager::BinderRegistry registry;
//   registry.AddInterface(factory.GetBinder<mojom::Foo>());
//   registry.AddInterface(factory.GetBinder<mojom::Bar>());
//
//   auto bad_binder = factory.GetBinder<mojom::SomeOtherType>();
//   ASSERT_TRUE(bad_binder.is_null());
class MultiBinderFactory {
 public:
  MultiBinderFactory();
  MultiBinderFactory(const MultiBinderFactory&) = delete;
  ~MultiBinderFactory();
  MultiBinderFactory& operator=(const MultiBinderFactory&) = delete;

  template <typename Interface>
  void AddInterface(Interface* interface) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto result = binder_factories_.emplace(
        Interface::Name_,
        std::make_unique<BinderFactory<Interface>>(interface));
    DCHECK(result.second);
  }

  template <typename Interface>
  void AddSingleBinderInterface(Interface* interface) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto result = binder_factories_.emplace(
        Interface::Name_, std::make_unique<BinderFactory<Interface>>(
                              interface, nullptr, true /* single_binder */));
    DCHECK(result.second);
  }

  template <typename Interface>
  void AddBinder(
      const base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>&
          callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto result = binder_factories_.emplace(
        Interface::Name_, std::make_unique<BinderCallbackWrapper<Interface>>(
                              callback, task_runner));
    DCHECK(result.second);
  }

  bool HasInterface(const std::string& interface_name) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = binder_factories_.find(interface_name);
    return it != binder_factories_.end();
  }

  template <typename Interface>
  bool HasInterface() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return HasInterface(Interface::Name_);
  }

  template <typename Interface>
  void RemoveInterface() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = binder_factories_.find(Interface::Name_);
    if (it == binder_factories_.end()) {
      return;
    }
    binder_factories_.erase(it);
  }

  template <typename Interface>
  base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)> GetBinder() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = binder_factories_.find(Interface::Name_);
    if (it == binder_factories_.end()) {
      return base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>();
    }
    return base::BindRepeating(&ForwardToPipeBinder<Interface>,
                               it->second->GetPipeBinder());
  }

  template <typename Interface>
  bool Bind(mojo::PendingReceiver<Interface> request) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return Bind(Interface::Name_, request.PassPipe());
  }

  bool Bind(const std::string& interface_name,
            mojo::ScopedMessagePipeHandle handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = binder_factories_.find(interface_name);
    if (it == binder_factories_.end()) {
      return false;
    }
    it->second->BindPipe(std::move(handle));
    return true;
  }

 private:
  template <typename Interface>
  static void ForwardToPipeBinder(
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> pipe_binder,
      mojo::PendingReceiver<Interface> request) {
    pipe_binder.Run(request.PassPipe());
  }

  base::flat_map<std::string, std::unique_ptr<BinderFactoryBase>>
      binder_factories_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chromecast

#endif  // CHROMECAST_MOJO_BINDER_FACTORY_H_
