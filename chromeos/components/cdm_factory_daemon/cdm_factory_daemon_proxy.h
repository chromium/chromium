// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_H_

#include "base/component_export.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/components/cdm_factory_daemon/mojom/browser_cdm_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

// Base class with simple functionality for handling the GPU->Browser,
// Lacros Browser->Ash Browser, and Ash Browser->ChromeOS Daemon communication
// for ChromeOS HW backed CDMs and OEMCrypto. Subclasses must implement the
// BrowserCdmFactory Mojo interface.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) CdmFactoryDaemonProxy
    : public cdm::mojom::BrowserCdmFactory {
 public:
  CdmFactoryDaemonProxy();

  CdmFactoryDaemonProxy(const CdmFactoryDaemonProxy&) = delete;
  CdmFactoryDaemonProxy& operator=(const CdmFactoryDaemonProxy&) = delete;

  ~CdmFactoryDaemonProxy() override;

 protected:
  void BindReceiver(mojo::PendingReceiver<BrowserCdmFactory> receiver);

  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;

 private:
  mojo::ReceiverSet<BrowserCdmFactory> receivers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_H_
