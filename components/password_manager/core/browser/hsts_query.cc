// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hsts_query.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

// Helper since a once-callback may need to be called from two paths.
class HSTSCallbackHelper : public base::RefCounted<HSTSCallbackHelper> {
 public:
  explicit HSTSCallbackHelper(HSTSCallback user_callback)
      : user_callback_(std::move(user_callback)) {}

  HSTSCallbackHelper(const HSTSCallbackHelper&) = delete;
  HSTSCallbackHelper& operator=(const HSTSCallbackHelper&) = delete;

  void ReportResult(bool result) {
    std::move(user_callback_).Run(result ? HSTSResult::kYes : HSTSResult::kNo);
  }

  void ReportError() { std::move(user_callback_).Run(HSTSResult::kError); }

 private:
  friend class base::RefCounted<HSTSCallbackHelper>;
  ~HSTSCallbackHelper() = default;

  HSTSCallback user_callback_;
};

}  // namespace

void PostHSTSQueryForHostAndNetworkContext(
    const url::Origin& origin,
    network::mojom::NetworkContext* network_context,
    HSTSCallback callback) {
  if (origin.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HSTSResult::kNo));
    return;
  }

  if (!network_context) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HSTSResult::kError));
    return;
  }

  scoped_refptr<HSTSCallbackHelper> callback_helper =
      base::MakeRefCounted<HSTSCallbackHelper>(std::move(callback));
  network_context->IsHSTSActiveForHost(
      origin.host(),
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(&HSTSCallbackHelper::ReportResult, callback_helper),
          base::BindOnce(&HSTSCallbackHelper::ReportError, callback_helper)));
}

}  // namespace password_manager
