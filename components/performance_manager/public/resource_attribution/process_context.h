// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PROCESS_CONTEXT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PROCESS_CONTEXT_H_

#include <compare>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {
class BrowserChildProcessHost;
class RenderProcessHost;
}  // namespace content

namespace performance_manager {
class ProcessNode;
}

namespace performance_manager::resource_attribution {

class ProcessContext {
 public:
  ~ProcessContext();

  ProcessContext(const ProcessContext& other);
  ProcessContext& operator=(const ProcessContext& other);
  ProcessContext(ProcessContext&& other);
  ProcessContext& operator=(ProcessContext&& other);

  // UI thread methods.

  // Returns the ProcessContext for the browser process, or nullopt if there is
  // none. (This could happen in tests, or before the PerformanceManager
  // starts.)
  static absl::optional<ProcessContext> FromBrowserProcess();

  // Returns the ProcessContext for the renderer process hosted in `host`, which
  // must be non-null and have a valid RenderProcessHostId. Returns nullopt if
  // the RenderProcessHost is not registered with PerformanceManager.
  static absl::optional<ProcessContext> FromRenderProcessHost(
      content::RenderProcessHost* host);

  // Returns the ProcessContext for the non-renderer child process hosted in
  // `host`, which must be non-null and have a valid BrowserChildProcessHostId.
  // Returns nullopt if the BrowserChildProcessHost is not registered with
  // PerformanceManager.
  static absl::optional<ProcessContext> FromBrowserChildProcessHost(
      content::BrowserChildProcessHost* host);

  // Returns true iff this context refers to the browser process.
  bool IsBrowserProcessContext() const;

  // Returns true iff this context refers to a renderer process.
  bool IsRenderProcessContext() const;

  // Returns true iff this context refers to a non-renderer child process.
  bool IsBrowserChildProcessContext() const;

  // If this context refers to a renderer process, returns its
  // RenderProcessHost. Returns nullptr if it is not a renderer process or the
  // host no longer exists.
  content::RenderProcessHost* GetRenderProcessHost() const;

  // If this context refers to a renderer process, returns the
  // RenderProcessHostId that was assigned to it, otherwise returns a null
  // RenderProcessHostId.
  RenderProcessHostId GetRenderProcessHostId() const;

  // If this context refers to a non-renderer child process, returns its
  // BrowserChildProcessHost. Returns nullptr if it is not a non-renderer child
  // process or the host no longer exists.
  content::BrowserChildProcessHost* GetBrowserChildProcessHost() const;

  // If this context refers to a non-renderer child process, returns
  // the BrowserChildProcessHostId that was assigned to it, otherwise returns a
  // null BrowserChildProcessHostId.
  BrowserChildProcessHostId GetBrowserChildProcessHostId() const;

  // Returns the ProcessNode for this context, or a null WeakPtr if it no longer
  // exists.
  base::WeakPtr<ProcessNode> GetWeakProcessNode() const;

  // PM sequence methods.

  // Returns the ProcessContext for `node`. Equivalent to
  // node->GetResourceContext().
  static ProcessContext FromProcessNode(const ProcessNode* node);

  // Returns the ProcessContext for `node`, or nullopt if `node` is null.
  static absl::optional<ProcessContext> FromWeakProcessNode(
      base::WeakPtr<ProcessNode> node);

  // Returns the ProcessNode for this context, or nullptr if it no longer
  // exists.
  ProcessNode* GetProcessNode() const;

  // Returns a string representation of the context for debugging. This matches
  // the interface of base::TokenType and base::UnguessableToken, for
  // convenience.
  std::string ToString() const;

  // Compare ProcessContexts by process host id.
  friend std::strong_ordering operator<=>(const ProcessContext& a,
                                          const ProcessContext& b) {
    // absl::variant doesn't define <=>.
    if (a.id_ < b.id_) {
      return std::strong_ordering::less;
    }
    if (a.id_ == b.id_) {
      return std::strong_ordering::equal;
    }
    return std::strong_ordering::greater;
  }

  // Test ProcessContexts for equality by process host id.
  friend bool operator==(const ProcessContext& a, const ProcessContext& b) {
    return a.id_ == b.id_;
  }

 private:
  // A tag for the browser process which has no id.
  struct BrowserProcessTag {
    friend constexpr auto operator<=>(const BrowserProcessTag&,
                                      const BrowserProcessTag&) = default;
    friend constexpr bool operator==(const BrowserProcessTag&,
                                     const BrowserProcessTag&) = default;
  };
  static_assert(BrowserProcessTag{} == BrowserProcessTag{},
                "empty structs should always compare equal");

  using AnyProcessHostId = absl::variant<BrowserProcessTag,
                                         RenderProcessHostId,
                                         BrowserChildProcessHostId>;

  ProcessContext(AnyProcessHostId id, base::WeakPtr<ProcessNode> weak_node);

  AnyProcessHostId id_;
  base::WeakPtr<ProcessNode> weak_node_;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PROCESS_CONTEXT_H_
