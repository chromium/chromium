// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/graph_dump_impl.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/bind_post_task.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// Best effort convert |value| to a string.
std::string ToJSON(const base::Value::Dict& value) {
  std::string result;
  JSONStringValueSerializer serializer(&result);
  if (serializer.Serialize(value))
    return result;

  return std::string();
}

}  // namespace

class DiscardsGraphDumpImpl::FaviconRequestHelper {
 public:
  FaviconRequestHelper() = default;
  ~FaviconRequestHelper() = default;

  FaviconRequestHelper(const FaviconRequestHelper&) = delete;
  FaviconRequestHelper& operator=(const FaviconRequestHelper&) = delete;

  void RequestFavicon(GURL page_url,
                      base::WeakPtr<content::WebContents> web_contents,
                      FaviconAvailableCallback on_favicon_available);
  void FaviconDataAvailable(FaviconAvailableCallback on_favicon_available,
                            const favicon_base::FaviconRawBitmapResult& result);

 private:
  base::CancelableTaskTracker cancelable_task_tracker_;
  SEQUENCE_CHECKER(sequence_checker_);
};

void DiscardsGraphDumpImpl::FaviconRequestHelper::RequestFavicon(
    GURL page_url,
    base::WeakPtr<content::WebContents> web_contents,
    FaviconAvailableCallback on_favicon_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return;

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  constexpr size_t kIconSize = 16;
  constexpr bool kFallbackToHost = true;
  // It's safe to pass this unretained here, as the tasks are cancelled
  // on deletion of the cancelable task tracker.
  favicon_service->GetRawFaviconForPageURL(
      page_url, {favicon_base::IconType::kFavicon}, kIconSize, kFallbackToHost,
      base::BindOnce(&FaviconRequestHelper::FaviconDataAvailable,
                     base::Unretained(this), std::move(on_favicon_available)),
      &cancelable_task_tracker_);
}

void DiscardsGraphDumpImpl::FaviconRequestHelper::FaviconDataAvailable(
    FaviconAvailableCallback on_favicon_available,
    const favicon_base::FaviconRawBitmapResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.is_valid())
    return;
  std::move(on_favicon_available).Run(result.bitmap_data);
}

DiscardsGraphDumpImpl::DiscardsGraphDumpImpl() {}

DiscardsGraphDumpImpl::~DiscardsGraphDumpImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!change_subscriber_);
}

// static
void DiscardsGraphDumpImpl::CreateAndBind(
    mojo::PendingReceiver<discards::mojom::GraphDump> receiver,
    performance_manager::Graph* graph) {
  std::unique_ptr<DiscardsGraphDumpImpl> dump =
      std::make_unique<DiscardsGraphDumpImpl>();

  dump->BindWithGraph(graph, std::move(receiver));
  graph->PassToGraph(std::move(dump));
}

void DiscardsGraphDumpImpl::BindWithGraph(
    performance_manager::Graph* graph,
    mojo::PendingReceiver<discards::mojom::GraphDump> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &DiscardsGraphDumpImpl::OnConnectionError, base::Unretained(this)));
}

int64_t DiscardsGraphDumpImpl::GetNodeIdForTesting(
    const performance_manager::Node* node) {
  return GetNodeId(node);
}

namespace {

template <typename FunctionType>
void ForFrameAndOffspring(const performance_manager::FrameNode* parent_frame,
                          FunctionType on_frame) {
  on_frame(parent_frame);

  for (const performance_manager::FrameNode* child_frame :
       parent_frame->GetChildFrameNodes())
    ForFrameAndOffspring(child_frame, on_frame);
}

}  // namespace

void DiscardsGraphDumpImpl::SubscribeToChanges(
    mojo::PendingRemote<discards::mojom::GraphChangeStream> change_subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_subscriber_.Bind(std::move(change_subscriber));

  // Give all existing nodes an ID.
  performance_manager::Graph* graph = GetOwningGraph();
  for (const performance_manager::FrameNode* frame_node :
       graph->GetAllFrameNodes()) {
    AddNode(frame_node);
  }
  for (const performance_manager::PageNode* page_node :
       graph->GetAllPageNodes()) {
    AddNode(page_node);
  }
  for (const performance_manager::ProcessNode* process_node :
       graph->GetAllProcessNodes()) {
    AddNode(process_node);
  }
  for (const performance_manager::WorkerNode* worker_node :
       graph->GetAllWorkerNodes()) {
    AddNode(worker_node);
  }

  // Send creation notifications for all existing nodes.
  SendNotificationToAllNodes(/* created = */ true);

  // It is entirely possible for there to be circular link references between
  // nodes that already existed at the point this object was created (the loop
  // was closed after the two nodes themselves were created). We don't have the
  // exact order of historical events that led to the current graph state, so we
  // simply fire off a node changed notification for all nodes after the node
  // creation. This ensures that all targets exist the second time through, and
  // any loops are closed. Afterwards any newly created loops will be properly
  // maintained as node creation/destruction/link events will be fed to the
  // graph in the proper order.
  SendNotificationToAllNodes(/* created = */ false);

  // Subscribe to subsequent notifications.
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
}

void DiscardsGraphDumpImpl::RequestNodeDescriptions(
    const std::vector<int64_t>& node_ids,
    RequestNodeDescriptionsCallback callback) {
  base::flat_map<int64_t, std::string> descriptions;
  performance_manager::NodeDataDescriberRegistry* describer_registry =
      GetOwningGraph()->GetNodeDataDescriberRegistry();
  for (int64_t node_id : node_ids) {
    auto it = nodes_by_id_.find(NodeId::FromUnsafeValue(node_id));
    // The requested node may have been removed by the time the request arrives,
    // in which case no description is returned for that node ID.
    if (it != nodes_by_id_.end()) {
      descriptions[node_id] =
          ToJSON(describer_registry->DescribeNodeData(it->second));
    }
  }

  std::move(callback).Run(descriptions);
}

void DiscardsGraphDumpImpl::OnPassedToGraph(performance_manager::Graph* graph) {
  // Do nothing.
}

void DiscardsGraphDumpImpl::OnTakenFromGraph(
    performance_manager::Graph* graph) {
  if (change_subscriber_) {
    graph->RemoveFrameNodeObserver(this);
    graph->RemovePageNodeObserver(this);
    graph->RemoveProcessNodeObserver(this);
    graph->RemoveWorkerNodeObserver(this);
  }

  change_subscriber_.reset();
}

void DiscardsGraphDumpImpl::OnFrameNodeAdded(
    const performance_manager::FrameNode* frame_node) {
  AddNode(frame_node);
  SendFrameNotification(frame_node, true);
  StartFrameFaviconRequest(frame_node);
}

void DiscardsGraphDumpImpl::OnBeforeFrameNodeRemoved(
    const performance_manager::FrameNode* frame_node) {
  SendDeletionNotification(frame_node);
  RemoveNode(frame_node);
}

void DiscardsGraphDumpImpl::OnURLChanged(
    const performance_manager::FrameNode* frame_node,
    const GURL& previous_value) {
  SendFrameNotification(frame_node, false);
  StartFrameFaviconRequest(frame_node);
}

void DiscardsGraphDumpImpl::OnPageNodeAdded(
    const performance_manager::PageNode* page_node) {
  AddNode(page_node);
  SendPageNotification(page_node, true);
  StartPageFaviconRequest(page_node);
}

void DiscardsGraphDumpImpl::OnBeforePageNodeRemoved(
    const performance_manager::PageNode* page_node) {
  SendDeletionNotification(page_node);
  RemoveNode(page_node);
}

void DiscardsGraphDumpImpl::OnOpenerFrameNodeChanged(
    const performance_manager::PageNode* page_node,
    const performance_manager::FrameNode* previous_opener) {
  DCHECK(HasNode(page_node));
  SendPageNotification(page_node, false);
}

void DiscardsGraphDumpImpl::OnEmbedderFrameNodeChanged(
    const performance_manager::PageNode* page_node,
    const performance_manager::FrameNode*,
    EmbeddingType) {
  DCHECK(HasNode(page_node));
  SendPageNotification(page_node, false);
}

void DiscardsGraphDumpImpl::OnFaviconUpdated(
    const performance_manager::PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartPageFaviconRequest(page_node);
}

void DiscardsGraphDumpImpl::OnMainFrameUrlChanged(
    const performance_manager::PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendPageNotification(page_node, false);
}

void DiscardsGraphDumpImpl::OnProcessNodeAdded(
    const performance_manager::ProcessNode* process_node) {
  AddNode(process_node);
  SendProcessNotification(process_node, true);
}

void DiscardsGraphDumpImpl::OnProcessLifetimeChange(
    const performance_manager::ProcessNode* process_node) {
  SendProcessNotification(process_node, false);
}

void DiscardsGraphDumpImpl::OnBeforeProcessNodeRemoved(
    const performance_manager::ProcessNode* process_node) {
  SendDeletionNotification(process_node);
  RemoveNode(process_node);
}

void DiscardsGraphDumpImpl::OnWorkerNodeAdded(
    const performance_manager::WorkerNode* worker_node) {
  AddNode(worker_node);
  SendWorkerNotification(worker_node, true);
}

void DiscardsGraphDumpImpl::OnBeforeWorkerNodeRemoved(
    const performance_manager::WorkerNode* worker_node) {
  SendDeletionNotification(worker_node);
  RemoveNode(worker_node);
}

void DiscardsGraphDumpImpl::OnFinalResponseURLDetermined(
    const performance_manager::WorkerNode* worker_node) {
  SendWorkerNotification(worker_node, false);
}

void DiscardsGraphDumpImpl::OnBeforeClientFrameAdded(
    const performance_manager::WorkerNode* worker_node,
    const performance_manager::FrameNode* client_frame_node) {
  // Nothing to do.
}

void DiscardsGraphDumpImpl::OnClientFrameAdded(
    const performance_manager::WorkerNode* worker_node,
    const performance_manager::FrameNode* client_frame_node) {
  SendWorkerNotification(worker_node, false);
}

void DiscardsGraphDumpImpl::OnBeforeClientFrameRemoved(
    const performance_manager::WorkerNode* worker_node,
    const performance_manager::FrameNode* client_frame_node) {
  SendWorkerNotification(worker_node, false);
}

void DiscardsGraphDumpImpl::OnBeforeClientWorkerAdded(
    const performance_manager::WorkerNode* worker_node,
    const performance_manager::WorkerNode* client_worker_node) {
  // Nothing to do.
}

void DiscardsGraphDumpImpl::OnClientWorkerAdded(
    const performance_manager::WorkerNode* worker_node,
    const performance_manager::WorkerNode* client_worker_node) {
  SendWorkerNotification(worker_node, false);
}

void DiscardsGraphDumpImpl::OnBeforeClientWorkerRemoved(
    const performance_manager::WorkerNode* worker_node,
    const performance_manager::WorkerNode* client_worker_node) {
  SendWorkerNotification(worker_node, false);
}

void DiscardsGraphDumpImpl::AddNode(const performance_manager::Node* node) {
  DCHECK(node_ids_.find(node) == node_ids_.end());
  NodeId new_id = node_id_generator_.GenerateNextId();
  node_ids_.insert(std::make_pair(node, new_id));
  nodes_by_id_.insert(std::make_pair(new_id, node));
}

void DiscardsGraphDumpImpl::RemoveNode(const performance_manager::Node* node) {
  auto it = node_ids_.find(node);
  CHECK(it != node_ids_.end(), base::NotFatalUntil::M130);
  NodeId node_id = it->second;
  node_ids_.erase(it);
  size_t erased = nodes_by_id_.erase(node_id);
  DCHECK_EQ(1u, erased);
}

bool DiscardsGraphDumpImpl::HasNode(
    const performance_manager::Node* node) const {
  return node_ids_.find(node) != node_ids_.end();
}

int64_t DiscardsGraphDumpImpl::GetNodeId(
    const performance_manager::Node* node) const {
  if (node == nullptr)
    return 0;

  auto it = node_ids_.find(node);
  CHECK(it != node_ids_.end(), base::NotFatalUntil::M130);
  return it->second.GetUnsafeValue();
}

base::SequenceBound<DiscardsGraphDumpImpl::FaviconRequestHelper>&
DiscardsGraphDumpImpl::EnsureFaviconRequestHelper() {
  if (!favicon_request_helper_) {
    favicon_request_helper_ = base::SequenceBound<FaviconRequestHelper>(
        content::GetUIThreadTaskRunner({}));
  }
  return favicon_request_helper_;
}

DiscardsGraphDumpImpl::FaviconAvailableCallback
DiscardsGraphDumpImpl::GetFaviconAvailableCallback(int64_t serialization_id) {
  return base::BindPostTaskToCurrentDefault(
      base::BindOnce(&DiscardsGraphDumpImpl::SendFaviconNotification,
                     weak_factory_.GetWeakPtr(), serialization_id));
}

void DiscardsGraphDumpImpl::StartPageFaviconRequest(
    const performance_manager::PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!page_node->GetMainFrameUrl().is_valid())
    return;

  EnsureFaviconRequestHelper()
      .AsyncCall(&FaviconRequestHelper::RequestFavicon)
      .WithArgs(page_node->GetMainFrameUrl(), page_node->GetWebContents(),
                GetFaviconAvailableCallback(GetNodeId(page_node)));
}

void DiscardsGraphDumpImpl::StartFrameFaviconRequest(
    const performance_manager::FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!frame_node->GetURL().is_valid())
    return;

  EnsureFaviconRequestHelper()
      .AsyncCall(&FaviconRequestHelper::RequestFavicon)
      .WithArgs(frame_node->GetURL(),
                frame_node->GetPageNode()->GetWebContents(),
                GetFaviconAvailableCallback(GetNodeId(frame_node)));
}

void DiscardsGraphDumpImpl::SendNotificationToAllNodes(bool created) {
  performance_manager::Graph* graph = GetOwningGraph();
  for (const performance_manager::ProcessNode* process_node :
       graph->GetAllProcessNodes()) {
    SendProcessNotification(process_node, created);
  }

  for (const performance_manager::PageNode* page_node :
       graph->GetAllPageNodes()) {
    SendPageNotification(page_node, created);
    if (created)
      StartPageFaviconRequest(page_node);

    // Dispatch preorder frame notifications.
    for (const performance_manager::FrameNode* main_frame_node :
         page_node->GetMainFrameNodes()) {
      ForFrameAndOffspring(
          main_frame_node,
          [this, created](const performance_manager::FrameNode* frame_node) {
            this->SendFrameNotification(frame_node, created);
            if (created)
              this->StartFrameFaviconRequest(frame_node);
          });
    }
  }

  for (const performance_manager::WorkerNode* worker_node :
       graph->GetAllWorkerNodes()) {
    SendWorkerNotification(worker_node, created);
  }
}

void DiscardsGraphDumpImpl::SendFrameNotification(
    const performance_manager::FrameNode* frame,
    bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40109021): Add more frame properties.
  discards::mojom::FrameInfoPtr frame_info = discards::mojom::FrameInfo::New();

  frame_info->id = GetNodeId(frame);

  auto* parent_frame = frame->GetParentFrameNode();
  frame_info->parent_frame_id = GetNodeId(parent_frame);

  auto* process = frame->GetProcessNode();
  frame_info->process_id = GetNodeId(process);

  auto* page = frame->GetPageNode();
  frame_info->page_id = GetNodeId(page);

  frame_info->url = frame->GetURL();
  frame_info->description_json =
      ToJSON(GetOwningGraph()->GetNodeDataDescriberRegistry()->DescribeNodeData(
          frame));

  if (created) {
    change_subscriber_->FrameCreated(std::move(frame_info));
  } else {
    change_subscriber_->FrameChanged(std::move(frame_info));
  }
}

void DiscardsGraphDumpImpl::SendPageNotification(
    const performance_manager::PageNode* page_node,
    bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40109021): Add more page_node properties.
  discards::mojom::PageInfoPtr page_info = discards::mojom::PageInfo::New();

  page_info->id = GetNodeId(page_node);
  page_info->main_frame_url = page_node->GetMainFrameUrl();
  page_info->opener_frame_id = GetNodeId(page_node->GetOpenerFrameNode());
  page_info->embedder_frame_id = GetNodeId(page_node->GetEmbedderFrameNode());
  page_info->description_json =
      ToJSON(GetOwningGraph()->GetNodeDataDescriberRegistry()->DescribeNodeData(
          page_node));

  if (created)
    change_subscriber_->PageCreated(std::move(page_info));
  else
    change_subscriber_->PageChanged(std::move(page_info));
}

void DiscardsGraphDumpImpl::SendProcessNotification(
    const performance_manager::ProcessNode* process,
    bool created) {
  // TODO(crbug.com/40109021): Add more process properties.
  discards::mojom::ProcessInfoPtr process_info =
      discards::mojom::ProcessInfo::New();

  process_info->id = GetNodeId(process);
  process_info->pid = process->GetProcessId();
  process_info->private_footprint_kb = process->GetPrivateFootprintKb();

  process_info->description_json =
      ToJSON(GetOwningGraph()->GetNodeDataDescriberRegistry()->DescribeNodeData(
          process));

  if (created)
    change_subscriber_->ProcessCreated(std::move(process_info));
  else
    change_subscriber_->ProcessChanged(std::move(process_info));
}

void DiscardsGraphDumpImpl::SendWorkerNotification(
    const performance_manager::WorkerNode* worker,
    bool created) {
  // TODO(crbug.com/40109021): Add more process properties.
  discards::mojom::WorkerInfoPtr worker_info =
      discards::mojom::WorkerInfo::New();

  worker_info->id = GetNodeId(worker);
  worker_info->url = worker->GetURL();
  worker_info->process_id = GetNodeId(worker->GetProcessNode());

  for (const performance_manager::FrameNode* client_frame :
       worker->GetClientFrames()) {
    worker_info->client_frame_ids.push_back(GetNodeId(client_frame));
  }
  for (const performance_manager::WorkerNode* client_worker :
       worker->GetClientWorkers()) {
    worker_info->client_worker_ids.push_back(GetNodeId(client_worker));
  }
  for (const performance_manager::WorkerNode* child_worker :
       worker->GetChildWorkers()) {
    worker_info->child_worker_ids.push_back(GetNodeId(child_worker));
  }

  worker_info->description_json =
      ToJSON(GetOwningGraph()->GetNodeDataDescriberRegistry()->DescribeNodeData(
          worker));

  if (created)
    change_subscriber_->WorkerCreated(std::move(worker_info));
  else
    change_subscriber_->WorkerChanged(std::move(worker_info));
}

void DiscardsGraphDumpImpl::SendDeletionNotification(
    const performance_manager::Node* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_subscriber_->NodeDeleted(GetNodeId(node));
}

void DiscardsGraphDumpImpl::SendFaviconNotification(
    int64_t serialization_id,
    scoped_refptr<base::RefCountedMemory> bitmap_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(0u, bitmap_data->size());

  discards::mojom::FavIconInfoPtr icon_info =
      discards::mojom::FavIconInfo::New();
  icon_info->node_id = serialization_id;
  icon_info->icon_data = base::Base64Encode(*bitmap_data);

  change_subscriber_->FavIconDataAvailable(std::move(icon_info));
}

// static
void DiscardsGraphDumpImpl::OnConnectionError(DiscardsGraphDumpImpl* impl) {
  std::unique_ptr<GraphOwned> owned_impl =
      impl->GetOwningGraph()->TakeFromGraph(impl);
}
