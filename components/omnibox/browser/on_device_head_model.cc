// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_model.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <list>
#include <memory>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

namespace {
// The offset of the root node for the tree. The first two bytes is reserved to
// specify the size (num of bytes) of the address and the score in each node.
const int kRootNodeOffset = 2;

// A useful data structure to keep track of the tree nodes should be and have
// been visited during tree traversal.
struct MatchCandidate {
  // The sequences of characters from the start node to current node.
  std::string text;

  // Whether the text above can be returned as a suggestion; if false it is the
  // prefix of some other complete suggestion.
  bool is_complete_suggestion;

  // If is_complete_suggestion is true, this is the score for the suggestion;
  // Otherwise it will be set as the maximum score for its sub tree.
  uint32_t score;

  // The address of the node in the model file. It is not required if
  // is_complete_suggestion is true.
  uint32_t address;
};

// Doubly linked list structure, which will be sorted based on candidates'
// scores (from low to high), to track nodes during tree search. We use two of
// this list to keep max_num_matches_to_return_ nodes in total with highest
// score during the search, and prune children and branches with low score.
// In theory, using RBTree might give a better search performance
// (i.e. log(n)) compared with linear from linked list here when inserting new
// candidates with high score into the struct, but since n is usually small,
// using linked list shall be okay.
using CandidateQueue = std::list<MatchCandidate>;

// A mini class holds all parameters needed to access the model on disk.
class OnDeviceModelParams {
 public:
  static std::unique_ptr<OnDeviceModelParams> Create(
      const std::string& model_filename,
      const uint32_t max_num_matches_to_return);

  std::ifstream* GetModelFileStream() { return &model_filestream_; }
  uint32_t score_size() const { return score_size_; }
  uint32_t address_size() const { return address_size_; }
  uint32_t max_num_matches_to_return() const {
    return max_num_matches_to_return_;
  }

  ~OnDeviceModelParams();
  OnDeviceModelParams(const OnDeviceModelParams&) = delete;
  OnDeviceModelParams& operator=(const OnDeviceModelParams&) = delete;

 private:
  OnDeviceModelParams() = default;

  std::ifstream model_filestream_;
  uint32_t score_size_;
  uint32_t address_size_;
  uint32_t max_num_matches_to_return_;
};

uint32_t ConvertCharSpanToInt(base::span<const char> chars) {
  CHECK_LE(chars.size(), sizeof(uint32_t));
  uint32_t result = 0;
  for (uint32_t i = 0; i < chars.size(); ++i) {
    result |= (chars[i] & 0xff) << (8 * i);
  }
  return result;
}

bool OpenModelFileStream(OnDeviceModelParams* params,
                         const std::string& model_filename,
                         const uint32_t start_address) {
  if (model_filename.empty()) {
    DVLOG(1) << "Model filename is empty";
    return false;
  }

  // First close the file if it's still open.
  if (params->GetModelFileStream()->is_open()) {
    DVLOG(1) << "Previous file is still open";
    params->GetModelFileStream()->close();
  }

  params->GetModelFileStream()->open(model_filename,
                                     std::ios::in | std::ios::binary);
  if (!params->GetModelFileStream()->is_open()) {
    DVLOG(1) << "Failed to open model file from [" << model_filename << "]";
    return false;
  }

  if (start_address > 0) {
    params->GetModelFileStream()->seekg(start_address);
  }
  return true;
}

void MaybeCloseModelFileStream(OnDeviceModelParams* params) {
  if (params->GetModelFileStream()->is_open()) {
    params->GetModelFileStream()->close();
  }
}

// Reads next num_bytes from the file stream.
bool ReadNext(OnDeviceModelParams* params, base::span<char> buf) {
  uint32_t address = params->GetModelFileStream()->tellg();
  params->GetModelFileStream()->read(buf.data(), buf.size());
  if (params->GetModelFileStream()->fail()) {
    DVLOG(1) << "On Device Head model: ifstream read error at address ["
             << address << "], when trying to read [" << buf.size()
             << "] bytes";
    return false;
  }
  return true;
}

// Reads next num_bytes from the file stream but returns as an integer.
uint32_t ReadNextNumBytesAsInt(OnDeviceModelParams* params,
                               uint32_t num_bytes,
                               bool* is_successful) {
  auto buf = base::HeapArray<char>::WithSize(num_bytes);
  *is_successful = ReadNext(params, buf);
  if (!*is_successful) {
    return 0;
  }

  return ConvertCharSpanToInt(buf);
}

// Checks if size of score and size of address read from the model file are
// valid.
// For score, we use size of 2 bytes (15 bits), 3 bytes (23 bits) or 4 bytes
// (31 bits); For address, we use size of 3 bytes (23 bits) or 4 bytes
// (31 bits).
bool AreSizesValid(OnDeviceModelParams* params) {
  bool is_score_size_valid =
      (params->score_size() >= 2 && params->score_size() <= 4);
  bool is_address_size_valid =
      (params->address_size() >= 3 && params->address_size() <= 4);
  if (!is_score_size_valid) {
    DVLOG(1) << "On Device Head model: score size [" << params->score_size()
             << "] is not valid; valid size should 2, 3 or 4 bytes.";
  }
  if (!is_address_size_valid) {
    DVLOG(1) << "On Device Head model: address size [" << params->address_size()
             << "] is not valid; valid size should be 3 or 4 bytes.";
  }
  return is_score_size_valid && is_address_size_valid;
}

void InsertCandidateToQueue(const MatchCandidate& candidate,
                            CandidateQueue* leaf_queue,
                            CandidateQueue* non_leaf_queue) {
  CandidateQueue* queue_ptr =
      candidate.is_complete_suggestion ? leaf_queue : non_leaf_queue;

  if (queue_ptr->empty() || candidate.score > queue_ptr->back().score) {
    queue_ptr->push_back(candidate);
  } else {
    auto iter = queue_ptr->begin();
    for (; iter != queue_ptr->end() && candidate.score > iter->score; ++iter) {
    }
    queue_ptr->insert(iter, candidate);
  }
}

uint32_t GetMinScoreFromQueues(OnDeviceModelParams* params,
                               const CandidateQueue& queue_1,
                               const CandidateQueue& queue_2) {
  uint32_t min_score = 0x1 << (params->score_size() * 8 - 1);
  if (!queue_1.empty()) {
    min_score = std::min(min_score, queue_1.front().score);
  }
  if (!queue_2.empty()) {
    min_score = std::min(min_score, queue_2.front().score);
  }
  return min_score;
}

// Reads block max_score_as_root at the beginning of the node from the given
// address. If there is a leaf score at the end of the block, return the leaf
// score using param leaf_candidate;
uint32_t ReadMaxScoreAsRoot(OnDeviceModelParams* params,
                            uint32_t address,
                            MatchCandidate* leaf_candidate,
                            bool* is_successful) {
  if (is_successful == nullptr) {
    DVLOG(1) << "On Device Head model: a boolean var is_successful is required "
             << "when calling function ReadMaxScoreAsRoot";
    return 0;
  }

  params->GetModelFileStream()->seekg(address);
  uint32_t max_score_block =
      ReadNextNumBytesAsInt(params, params->score_size(), is_successful);
  if (!*is_successful) {
    return 0;
  }

  // The 1st bit is the indicator so removing it when rebuilding the max
  // score as root.
  uint32_t max_score = max_score_block >> 1;

  // Read the leaf_score and set leaf_candidate when the indicator is 1.
  if ((max_score_block & 0x1) == 0x1 && leaf_candidate != nullptr) {
    uint32_t leaf_score =
        ReadNextNumBytesAsInt(params, params->score_size(), is_successful);
    if (!*is_successful) {
      return 0;
    }
    leaf_candidate->score = leaf_score;
    leaf_candidate->is_complete_suggestion = true;
  }
  return max_score;
}

// Reads a child block and move ifstream cursor to next child; returns false
// when reaching the end of the node or ifstream read error happens.
bool ReadNextChild(OnDeviceModelParams* params, MatchCandidate* candidate) {
  if (candidate == nullptr) {
    return false;
  }

  // Read block [length of text];
  bool is_successful;
  uint32_t text_length = ReadNextNumBytesAsInt(params, 1, &is_successful);
  if (!is_successful) {
    return false;
  }

  // This is the end of the node.
  if (text_length == 0) {
    return false;
  }

  // Read block [text].
  auto text_buf = base::HeapArray<char>::WithSize(text_length);
  if (!ReadNext(params, text_buf)) {
    return false;
  }
  std::string text(base::as_string_view(text_buf));
  // Append the text in this child such that the MatchCandidate object always
  // contains the string representing the path from the root node to here.
  candidate->text = base::StrCat({candidate->text, text});

  // Read block [1 bit indicator + address/leaf_score]
  // First read the 1 bit indicator.
  char first_byte;
  if (!ReadNext(params, base::span_from_ref(first_byte))) {
    return false;
  }
  bool is_leaf_score = (first_byte & 0x1) == 0x0;

  uint32_t length_of_leftover =
      (is_leaf_score ? params->score_size() : params->address_size()) - 1;

  auto leftover = base::HeapArray<char>::WithSize(length_of_leftover);
  is_successful = ReadNext(params, leftover);

  if (is_successful) {
    auto last_block = base::HeapArray<char>::WithSize(length_of_leftover + 1);
    last_block[0] = first_byte;
    last_block.last(length_of_leftover).copy_from(leftover);
    // Remove the 1 bit indicator when re-constructing the score/address.
    uint32_t score_or_address = ConvertCharSpanToInt(last_block) >> 1;

    if (is_leaf_score) {
      // Address is not required for leaf child.
      candidate->score = score_or_address;
      candidate->is_complete_suggestion = true;
    } else {
      candidate->address = score_or_address;
      candidate->is_complete_suggestion = false;

      // TODO(crbug.com/40947213): remove this guard after evaluating the fix.
      if (OmniboxFieldTrial::ShouldApplyOnDeviceHeadModelSelectionFix()) {
        MatchCandidate unused_candidate;
        uint32_t address = params->GetModelFileStream()->tellg();
        uint32_t max_score = ReadMaxScoreAsRoot(
            params, score_or_address, &unused_candidate, &is_successful);
        params->GetModelFileStream()->seekg(address);
        if (is_successful) {
          candidate->score = max_score;
        }
      }
    }
  }

  return is_successful;
}

// Reads tree node from given match candidate, convert all possible suggestions
// and children of this node into structure MatchCandidate.
std::vector<MatchCandidate> ReadTreeNode(OnDeviceModelParams* params,
                                         const MatchCandidate& current) {
  std::vector<MatchCandidate> candidates;
  // The current candidate passed in is a leaf node and we shall stop here.
  if (current.is_complete_suggestion) {
    return candidates;
  }

  bool is_successful;
  MatchCandidate leaf_candidate;
  leaf_candidate.is_complete_suggestion = false;

  uint32_t max_score_as_root = ReadMaxScoreAsRoot(
      params, current.address, &leaf_candidate, &is_successful);
  if (!is_successful) {
    DVLOG(1) << "On Device Head model: read max_score_as_root failed at "
             << "address [" << current.address << "]";
    return candidates;
  }

  // The max_score_as_root block may contain a leaf node which corresponds to a
  // valid suggestion. Its score was set in function ReadMaxScoreAsRoot.
  if (leaf_candidate.is_complete_suggestion) {
    leaf_candidate.text = current.text;
    candidates.push_back(leaf_candidate);
  }

  // Read child blocks until we reach the end of the node.
  while (true) {
    MatchCandidate candidate;
    candidate.text = current.text;
    candidate.score = max_score_as_root;
    if (!ReadNextChild(params, &candidate)) {
      break;
    }
    candidates.push_back(candidate);
  }
  return candidates;
}

// Finds start node which matches given prefix, returns true if found and the
// start node using param match_candidate.
bool FindStartNode(OnDeviceModelParams* params,
                   const std::string& prefix,
                   MatchCandidate* start_match) {
  if (start_match == nullptr) {
    return false;
  }

  start_match->text = "";
  start_match->score = 0;
  start_match->address = kRootNodeOffset;
  start_match->is_complete_suggestion = false;

  while (start_match->text.size() < prefix.size()) {
    auto children = ReadTreeNode(params, *start_match);
    bool has_match = false;
    for (auto const& child : children) {
      // The way we build the model ensures that there will be only one child
      // matching the given prefix at each node.
      if (!child.text.empty() &&
          (base::StartsWith(child.text, prefix, base::CompareCase::SENSITIVE) ||
           base::StartsWith(prefix, child.text,
                            base::CompareCase::SENSITIVE))) {
        // A leaf only partially matching the given prefix cannot be the right
        // start node.
        if (child.is_complete_suggestion && child.text.size() < prefix.size()) {
          continue;
        }
        start_match->text = child.text;
        start_match->is_complete_suggestion = child.is_complete_suggestion;
        start_match->score = child.score;
        start_match->address = child.address;
        has_match = true;
        break;
      }
    }
    if (!has_match) {
      return false;
    }
  }

  return start_match->text.size() >= prefix.size();
}

std::vector<std::pair<std::string, uint32_t>> DoSearch(
    OnDeviceModelParams* params,
    const MatchCandidate& start_match) {
  std::vector<std::pair<std::string, uint32_t>> suggestions;

  CandidateQueue leaf_queue, non_leaf_queue;
  uint32_t min_score_in_queues = start_match.score;
  InsertCandidateToQueue(start_match, &leaf_queue, &non_leaf_queue);

  // Do the search until there is no non leaf candidates in the queue.
  while (!non_leaf_queue.empty()) {
    // Always fetch the intermediate node with highest score at the back of the
    // queue.
    auto next_candidates = ReadTreeNode(params, non_leaf_queue.back());
    non_leaf_queue.pop_back();
    min_score_in_queues =
        GetMinScoreFromQueues(params, leaf_queue, non_leaf_queue);

    for (const auto& candidate : next_candidates) {
      if (candidate.score > min_score_in_queues ||
          (leaf_queue.size() + non_leaf_queue.size() <
           params->max_num_matches_to_return())) {
        InsertCandidateToQueue(candidate, &leaf_queue, &non_leaf_queue);
      }

      // If there are too many candidates in the queues, remove the one with
      // lowest score since it will never be shown to users.
      if (leaf_queue.size() + non_leaf_queue.size() >
          params->max_num_matches_to_return()) {
        if (leaf_queue.empty() ||
            (!non_leaf_queue.empty() &&
             leaf_queue.front().score > non_leaf_queue.front().score)) {
          non_leaf_queue.pop_front();
        } else {
          leaf_queue.pop_front();
        }
      }
      min_score_in_queues =
          GetMinScoreFromQueues(params, leaf_queue, non_leaf_queue);
    }
  }

  while (!leaf_queue.empty()) {
    suggestions.emplace_back(leaf_queue.back().text, leaf_queue.back().score);
    leaf_queue.pop_back();
  }

  return suggestions;
}

}  // namespace

// static
std::unique_ptr<OnDeviceModelParams> OnDeviceModelParams::Create(
    const std::string& model_filename,
    const uint32_t max_num_matches_to_return) {
  std::unique_ptr<OnDeviceModelParams> params(new OnDeviceModelParams());

  // TODO(crbug.com/40610979): Add DCHECK and code to report failures to UMA
  // histogram.
  if (!OpenModelFileStream(params.get(), model_filename, 0)) {
    DVLOG(1) << "On Device Head Params: cannot access on device head params "
             << "instance because model file cannot be opened";
    return nullptr;
  }

  char sizes[2];
  if (!ReadNext(params.get(), sizes)) {
    DVLOG(1) << "On Device Head Params: failed to read size information in the "
             << "first 2 bytes of the model file: " << model_filename;
    return nullptr;
  }

  params->address_size_ = sizes[0];
  params->score_size_ = sizes[1];
  if (!AreSizesValid(params.get())) {
    return nullptr;
  }

  params->max_num_matches_to_return_ = max_num_matches_to_return;
  return params;
}

OnDeviceModelParams::~OnDeviceModelParams() {
  if (model_filestream_.is_open()) {
    model_filestream_.close();
  }
}

// static
std::vector<std::pair<std::string, uint32_t>>
OnDeviceHeadModel::GetSuggestionsForPrefix(const std::string& model_filename,
                                           uint32_t max_num_matches_to_return,
                                           const std::string& prefix) {
  std::vector<std::pair<std::string, uint32_t>> suggestions;
  if (prefix.empty() || max_num_matches_to_return < 1) {
    return suggestions;
  }

  std::unique_ptr<OnDeviceModelParams> params =
      OnDeviceModelParams::Create(model_filename, max_num_matches_to_return);

  if (params && params->GetModelFileStream()->is_open()) {
    params->GetModelFileStream()->seekg(kRootNodeOffset);
    MatchCandidate start_match;
    if (FindStartNode(params.get(), prefix, &start_match)) {
      suggestions = DoSearch(params.get(), start_match);
    }
    MaybeCloseModelFileStream(params.get());
  }
  return suggestions;
}
