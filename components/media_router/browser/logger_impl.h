// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_LOGGER_IMPL_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_LOGGER_IMPL_H_

#include <string_view>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

class LoggerImpl : mojom::Logger {
 public:
  enum class Severity { kInfo, kWarning, kError };

  LoggerImpl();
  ~LoggerImpl() override;
  LoggerImpl(const LoggerImpl&) = delete;
  LoggerImpl& operator=(const LoggerImpl&) = delete;

  // mojom::Logger overrides:
  void LogInfo(mojom::LogCategory category,
               const std::string& component,
               const std::string& message,
               const std::string& sink_id,
               const std::string& media_source,
               const std::string& session_id) override;
  void LogWarning(mojom::LogCategory category,
                  const std::string& component,
                  const std::string& message,
                  const std::string& sink_id,
                  const std::string& media_source,
                  const std::string& session_id) override;
  void LogError(mojom::LogCategory category,
                const std::string& component,
                const std::string& message,
                const std::string& sink_id,
                const std::string& media_source,
                const std::string& session_id) override;
  void BindReceiver(mojo::PendingReceiver<mojom::Logger> receiver) override;

  // Called by tests or in-regular use that want to specify `time`.
  void Log(Severity severity,
           mojom::LogCategory category,
           base::Time time,
           const std::string& component,
           const std::string& message,
           const std::string& sink_id,
           const std::string& media_source,
           const std::string& session_id);

  std::string GetLogsAsJson() const;
  base::Value GetLogsAsValue() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(LoggerImplTest, RecordAndGetLogs);

  struct Entry {
   public:
    Entry(Severity severity,
          mojom::LogCategory category,
          base::Time time,
          std::string_view component,
          std::string_view message,
          std::string_view sink_id,
          std::string media_source,
          std::string_view session_id);
    Entry(Entry&& other);
    Entry(const Entry&) = delete;
    ~Entry();

    Severity severity;
    mojom::LogCategory category;
    base::Time time;
    // This is usually the name of the class that is emitting the log.
    std::string component;
    std::string message;
    // May be empty if the entry is not associated with a sink.
    std::string sink_id;
    // May be empty if the entry is not associated with a media source.
    std::string media_source;
    // May be empty if the entry is not associated with a session.
    std::string session_id;
  };

  static base::Value::Dict AsValue(const Entry& entry);

  mojo::ReceiverSet<mojom::Logger> receivers_;
  base::circular_deque<Entry> entries_;
  size_t const capacity_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_LOGGER_IMPL_H_
