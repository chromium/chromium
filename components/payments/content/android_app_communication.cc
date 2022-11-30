// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace payments {
namespace {

const char kAndroidAppCommunicationKeyName[] =
    "payment_android_app_communication";

}  // namespace

// static
base::WeakPtr<AndroidAppCommunication>
AndroidAppCommunication::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context);

  base::SupportsUserData::Data* data =
      context->GetUserData(kAndroidAppCommunicationKeyName);
  if (!data) {
    auto communication = AndroidAppCommunication::Create(context);
    data = communication.get();
    context->SetUserData(kAndroidAppCommunicationKeyName,
                         std::move(communication));
  }
  return static_cast<AndroidAppCommunication*>(data)
      ->weak_ptr_factory_.GetWeakPtr();
}

AndroidAppCommunication::~AndroidAppCommunication() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

AndroidAppCommunication::AndroidAppCommunication(
    content::BrowserContext* context)
    : context_(context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context_);
}

}  // namespace payments
