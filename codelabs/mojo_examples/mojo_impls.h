// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CODELABS_MOJO_EXAMPLES_MOJO_IMPLS_H_
#define CODELABS_MOJO_EXAMPLES_MOJO_IMPLS_H_

#include "base/task/single_thread_task_runner.h"
#include "codelabs/mojo_examples/mojom/interface.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

// The classes here are implementations of the mojo interfaces in this
// directory that some of the examples use. They are global instances whose
// receivers get bound to task runners associated with different task queues.
// `ObjectAImpl` is bound to a task queue that is initially frozen, and
// `ObjectBImpl` is bound to the current thread's default task runner,
// associated with an unfrozen task queue.
class ObjectAImpl : public codelabs::mojom::ObjectA {
 public:
  ObjectAImpl();
  ~ObjectAImpl() override;
  void BindToFrozenTaskRunner(
      mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectA>
          pending_receiver,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_tq_runner);

 private:
  // codelabs::mojom::ObjectA
  void DoA() override;

  mojo::AssociatedReceiver<codelabs::mojom::ObjectA> receiver_{this};
};

// The global instance of this class is bound to an unfrozen task queue, but
// doesn't receive any messages until the frozen task queue that manages
// `ObjectAImpl`'s is finally unfroze, after a delay.
class ObjectBImpl : public codelabs::mojom::ObjectB {
 public:
  ObjectBImpl();
  ~ObjectBImpl() override;
  void Bind(mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectB>
                pending_receiver);

 private:
  // codelabs::mojom::ObjectB
  void DoB() override;

  mojo::AssociatedReceiver<codelabs::mojom::ObjectB> receiver_{this};
};

#endif  // CODELABS_MOJO_EXAMPLES_MOJO_IMPLS_H_
