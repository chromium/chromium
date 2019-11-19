// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_JS_CHANNEL_SERVICE_H_
#define CHROMECAST_BROWSER_WEBVIEW_JS_CHANNEL_SERVICE_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list_types.h"
#include "chromecast/common/mojom/js_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace chromecast {

using JsChannelCallback =
    base::RepeatingCallback<void(const std::string&, const std::string&)>;

class JsChannelService : public mojom::JsChannelBindingProvider {
 public:
  static void Create(
      content::RenderProcessHost* render_process_host,
      mojo::PendingReceiver<mojom::JsChannelBindingProvider> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  explicit JsChannelService(int process_id);
  ~JsChannelService() override;

 private:
  // mojom::JsChannelBindingProvider implementation:
  void Register(int routing_id,
                mojo::PendingRemote<mojom::JsChannelClient> client) override;

  int process_id_;

  DISALLOW_COPY_AND_ASSIGN(JsChannelService);
};

class JsClientInstance {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnJsClientInstanceRegistered(int process_id,
                                              int routing_id,
                                              JsClientInstance* instance) = 0;
  };

  void AddChannel(const std::string& channel, JsChannelCallback callback);
  void RemoveChannel(const std::string& channel);

  static JsClientInstance* Find(int process_id, int routing_id);

  static void AddObserver(Observer* observer);
  static void RemoveObserver(Observer* observer);

 private:
  friend class JsChannelService;

  // These are created by JsChannelService.
  JsClientInstance(int process_id,
                   int routing_id,
                   mojo::PendingRemote<mojom::JsChannelClient> client);
  ~JsClientInstance();

  mojo::Remote<mojom::JsChannelClient> client_;
  mojo::UniqueReceiverSet<mojom::JsChannel> channels_;

  DISALLOW_COPY_AND_ASSIGN(JsClientInstance);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_JS_CHANNEL_SERVICE_H_
