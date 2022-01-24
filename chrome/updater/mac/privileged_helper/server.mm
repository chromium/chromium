// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/privileged_helper/server.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/updater/updater_branding.h"

namespace updater {

PrivilegedHelperServer::PrivilegedHelperServer()
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      service_(base::MakeRefCounted<PrivilegedHelperService>()) {}
PrivilegedHelperServer::~PrivilegedHelperServer() = default;

void PrivilegedHelperServer::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_delegate_.reset([[PrivilegedHelperServiceXPCDelegate alloc]
      initWithService:service_
               server:scoped_refptr<PrivilegedHelperServer>(this)]);
  service_listener_.reset([[NSXPCListener alloc]
      initWithMachServiceName:base::SysUTF8ToNSString(PRIVILEGED_HELPER_NAME)]);
  service_listener_.get().delegate = service_delegate_.get();

  [service_listener_ resume];
}

void PrivilegedHelperServer::FirstTaskRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrivilegedHelperServer::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_delegate_.reset();
  service_listener_.reset();
}

scoped_refptr<App> PrivilegedHelperServerInstance() {
  return base::MakeRefCounted<PrivilegedHelperServer>();
}

}  // namespace updater
