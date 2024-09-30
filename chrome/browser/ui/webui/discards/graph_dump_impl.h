// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISCARDS_GRAPH_DUMP_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_DISCARDS_GRAPH_DUMP_IMPL_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/types/id_type.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DiscardsGraphDumpImpl
    : public discards::mojom::GraphDump,
      public performance_manager::GraphOwned,
      public performance_manager::FrameNode::ObserverDefaultImpl,
      public performance_manager::PageNode::ObserverDefaultImpl,
      public performance_manager::ProcessNode::ObserverDefaultImpl,
      public performance_manager::WorkerNode::ObserverDefaultImpl {
 public:
  DiscardsGraphDumpImpl();

  DiscardsGraphDumpImpl(const DiscardsGraphDumpImpl&) = delete;
  DiscardsGraphDumpImpl& operator=(const DiscardsGraphDumpImpl&) = delete;

  ~DiscardsGraphDumpImpl() override;

  // Creates a new DiscardsGraphDumpImpl to service |receiver| and passes its
  // ownership to |graph|.
  static void CreateAndBind(
      mojo::PendingReceiver<discards::mojom::GraphDump> receiver,
      performance_manager::Graph* graph);

  // Exposed for testing.
  void BindWithGraph(
      performance_manager::Graph* graph,
      mojo::PendingReceiver<discards::mojom::GraphDump> receiver);

  int64_t GetNodeIdForTesting(const performance_manager::Node* node);

 protected:
  // WebUIGraphDump implementation.
  void SubscribeToChanges(
      mojo::PendingRemote<discards::mojom::GraphChangeStream> change_subscriber)
      override;
  void RequestNodeDescriptions(
      const std::vector<int64_t>& node_ids,
      RequestNodeDescriptionsCallback callback) override;

  // GraphOwned implementation.
  void OnPassedToGraph(performance_manager::Graph* graph) override;
  void OnTakenFromGraph(performance_manager::Graph* graph) override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(
      const performance_manager::FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(
      const performance_manager::FrameNode* frame_node) override;
  void OnURLChanged(const performance_manager::FrameNode* frame_node,
                    const GURL& previous_value) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const performance_manager::PageNode* page_node) override;
  void OnBeforePageNodeRemoved(
      const performance_manager::PageNode* page_node) override;
  void OnOpenerFrameNodeChanged(
      const performance_manager::PageNode* page_node,
      const performance_manager::FrameNode* previous_opener) override;
  void OnEmbedderFrameNodeChanged(
      const performance_manager::PageNode* page_node,
      const performance_manager::FrameNode* previous_embedder,
      EmbeddingType previous_embedding_type) override;
  void OnMainFrameUrlChanged(
      const performance_manager::PageNode* page_node) override;
  void OnFaviconUpdated(
      const performance_manager::PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(
      const performance_manager::ProcessNode* process_node) override;
  void OnProcessLifetimeChange(
      const performance_manager::ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(
      const performance_manager::ProcessNode* process_node) override;

  // WorkerNodeObserver implementation:
  void OnWorkerNodeAdded(
      const performance_manager::WorkerNode* worker_node) override;
  void OnBeforeWorkerNodeRemoved(
      const performance_manager::WorkerNode* worker_node) override;
  void OnFinalResponseURLDetermined(
      const performance_manager::WorkerNode* worker_node) override;
  void OnBeforeClientFrameAdded(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::FrameNode* client_frame_node) override;
  void OnClientFrameAdded(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::FrameNode* client_frame_node) override;
  void OnBeforeClientFrameRemoved(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::FrameNode* client_frame_node) override;
  void OnBeforeClientWorkerAdded(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::WorkerNode* client_worker_node) override;
  void OnClientWorkerAdded(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::WorkerNode* client_worker_node) override;
  void OnBeforeClientWorkerRemoved(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::WorkerNode* client_worker_node) override;

 private:
  // The favicon requests happen on the UI thread. This helper class
  // maintains the state required to do that.
  class FaviconRequestHelper;
  using FaviconAvailableCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;

  using NodeId = base::IdType64<class NodeIdTag>;

  void AddNode(const performance_manager::Node* node);
  void RemoveNode(const performance_manager::Node* node);
  bool HasNode(const performance_manager::Node* node) const;
  int64_t GetNodeId(const performance_manager::Node* node) const;

  base::SequenceBound<FaviconRequestHelper>& EnsureFaviconRequestHelper();

  // Returns a callback that will invoke SendFaviconNotification on the graph
  // sequence with the given `serialization_id`.
  FaviconAvailableCallback GetFaviconAvailableCallback(
      int64_t serialization_id);

  void StartPageFaviconRequest(const performance_manager::PageNode* page_node);
  void StartFrameFaviconRequest(
      const performance_manager::FrameNode* frame_node);

  void SendNotificationToAllNodes(bool created);
  void SendFrameNotification(const performance_manager::FrameNode* frame,
                             bool created);
  void SendPageNotification(const performance_manager::PageNode* page,
                            bool created);
  void SendProcessNotification(const performance_manager::ProcessNode* process,
                               bool created);
  void SendWorkerNotification(const performance_manager::WorkerNode* worker,
                              bool created);
  void SendDeletionNotification(const performance_manager::Node* node);
  void SendFaviconNotification(
      int64_t serialization_id,
      scoped_refptr<base::RefCountedMemory> bitmap_data);

  static void OnConnectionError(DiscardsGraphDumpImpl* impl);

  // Helper that requests favicons on the UI thread. Initialized to null to
  // avoid posting an initialization task to the UI thread during startup.
  // Access this through EnsureFaviconRequestHelper to initialize it on first
  // use.
  base::SequenceBound<FaviconRequestHelper> favicon_request_helper_;

  // The live nodes and their IDs.
  base::flat_map<const performance_manager::Node*, NodeId> node_ids_;
  base::flat_map<NodeId,
                 raw_ptr<const performance_manager::Node, CtnExperimental>>
      nodes_by_id_;
  NodeId::Generator node_id_generator_;

  // The current change subscriber to this dumper. This instance is subscribed
  // to every node in the graph save for the system node, so long as there is a
  // subscriber.
  mojo::Remote<discards::mojom::GraphChangeStream> change_subscriber_;
  mojo::Receiver<discards::mojom::GraphDump> receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DiscardsGraphDumpImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DISCARDS_GRAPH_DUMP_IMPL_H_
