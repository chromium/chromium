// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_SOCKET_SERVICE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_SOCKET_SERVICE_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cast_channel {

// Manages, opens, and closes CastSockets.
// This class may be created on any thread. All methods, unless otherwise noted,
// must be invoked on the SequencedTaskRunner given by |task_runner_|.
class CastSocketService {
 public:
  static CastSocketService* GetInstance();

  CastSocketService();
  virtual ~CastSocketService() = 0;

  // Removes the CastSocket corresponding to |channel_id| from the
  // CastSocketRegistry. Returns nullptr if no such CastSocket exists.
  virtual std::unique_ptr<CastSocket> RemoveSocket(int channel_id) = 0;

  // Attempts to close an open CastSocket connection corresponding to the given
  // |ip_endpoint|. Does nothing if the socket_id doesn't exist.
  virtual void CloseSocket(int channel_id);

  // Returns the socket corresponding to |channel_id| if one exists, or nullptr
  // otherwise.
  virtual CastSocket* GetSocket(int channel_id) const = 0;

  virtual CastSocket* GetSocket(const net::IPEndPoint& ip_endpoint) const = 0;

  // Opens cast socket with |open_params| and invokes |open_cb| when opening
  // operation finishes. If cast socket with |ip_endpoint| already exists,
  // invoke |open_cb| directly with the existing socket.
  // It is the caller's responsibility to ensure |open_params.ip_address| is
  // a valid private IP address as determined by |IsValidCastIPAddress()|.
  // |open_params|: Parameters necessary to open a Cast channel.
  // |open_cb|: OnOpenCallback invoked when cast socket is opened.
  // |network_context_getter| is called on UI thread only.
  virtual void OpenSocket(network::NetworkContextGetter network_context_getter,
                          const CastSocketOpenParams& open_params,
                          CastSocket::OnOpenCallback open_cb) = 0;

  // Adds |observer| to socket service. When socket service opens cast socket,
  // it passes |observer| to opened socket.
  // Does not take ownership of |observer|.
  virtual void AddObserver(CastSocket::Observer* observer) = 0;

  // Remove |observer| from each socket in |sockets_|
  virtual void RemoveObserver(CastSocket::Observer* observer) = 0;

  // Returns a pointer to the Logger member variable.
  scoped_refptr<cast_channel::Logger> GetLogger() { return logger_; }

  // Gets the TaskRunner for accessing this instance. Can be called from any
  // thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

  void SetTaskRunnerForTest(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    task_runner_ = task_runner;
  }

  // Allow test to inject a mock cast socket.
  void SetSocketForTest(std::unique_ptr<CastSocket> socket_for_test) {
    socket_for_test_ = std::move(socket_for_test);
  }

 protected:
  scoped_refptr<Logger> logger_;

  // The task runner on which |this| runs.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<CastSocket> socket_for_test_;
};

class CastSocketServiceImpl : public CastSocketService {
 public:
  CastSocketServiceImpl(const CastSocketServiceImpl&) = delete;
  CastSocketServiceImpl& operator=(const CastSocketServiceImpl&) = delete;

  ~CastSocketServiceImpl() override;

  // CastSocketService overrides.
  std::unique_ptr<CastSocket> RemoveSocket(int channel_id) override;
  CastSocket* GetSocket(int channel_id) const override;
  CastSocket* GetSocket(const net::IPEndPoint& ip_endpoint) const override;
  void OpenSocket(network::NetworkContextGetter network_context_getter,
                  const CastSocketOpenParams& open_params,
                  CastSocket::OnOpenCallback open_cb) override;
  void AddObserver(CastSocket::Observer* observer) override;
  void RemoveObserver(CastSocket::Observer* observer) override;

 private:
  friend class CastSocketService;
  friend class CastSocketServiceTest;
  friend class MockCastSocketService;

  using Sockets = std::map<int, std::unique_ptr<CastSocket>>;

  CastSocketServiceImpl();

  // Adds |socket| to |sockets_| and returns raw pointer of |socket|. Takes
  // ownership of |socket|.
  CastSocket* AddSocket(std::unique_ptr<CastSocket> socket);

  // Used to generate CastSocket id.
  static int last_channel_id_;

  // The collection of CastSocket keyed by channel_id.
  Sockets sockets_;

  // List of socket observers.
  base::ObserverList<CastSocket::Observer> observers_;
};

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_SOCKET_SERVICE_H_
