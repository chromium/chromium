// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/util.h"

#include <set>

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

// Note that updating the use case prefixes below will change the pre-existing
// per-user stickiness. Some of the derived data may already have been persisted
// elsewhere. Be sure you are aware of the implications before updating those
// strings. Note also that the version here is just about the hash method, and
// is distinctive from the broader configuration version of the Topics API.
const char kRandomOrTopTopicDecisionPrefix[] =
    "TopicsV1_RandomOrTopTopicDecision|";
const char kRandomTopicIndexDecisionPrefix[] =
    "TopicsV1_RandomTopicIndexDecision|";
const char kTopTopicIndexDecisionPrefix[] = "TopicsV1_TopTopicIndexDecision|";
const char kEpochSwitchTimeDecisionPrefix[] =
    "TopicsV1_EpochSwitchTimeDecision|";
const char kContextDomainStoragePrefix[] = "TopicsV1_ContextDomainStorage|";
const char kMainFrameHostStoragePrefix[] = "TopicsV1_MainFrameHostStorage|";

uint64_t HmacHash(ReadOnlyHmacKey hmac_key,
                  const std::string& use_case_prefix,
                  const std::string& data) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(hmac_key));

  uint64_t result;
  CHECK(hmac.Sign(use_case_prefix + data,
                  reinterpret_cast<unsigned char*>(&result), sizeof(result)));

  return result;
}

bool g_hmac_key_overridden = false;
  
browsing_topics::HmacKey& GetHmacKeyOverrideForTesting() {
  static browsing_topics::HmacKey key;
  return key;
}

std::map<Topic, Topic> GetChildToParentTopicMap() {
  if (blink::features::kBrowsingTopicsTaxonomyVersion.Get() != 1) {
    return {};
  }
  std::map<Topic, Topic> child_to_parent_map(
      {{Topic(2), Topic(1)},     {Topic(3), Topic(1)},
       {Topic(4), Topic(1)},     {Topic(5), Topic(1)},
       {Topic(6), Topic(1)},     {Topic(7), Topic(1)},
       {Topic(8), Topic(7)},     {Topic(9), Topic(1)},
       {Topic(10), Topic(1)},    {Topic(11), Topic(1)},
       {Topic(12), Topic(1)},    {Topic(13), Topic(12)},
       {Topic(14), Topic(12)},   {Topic(15), Topic(12)},
       {Topic(16), Topic(12)},   {Topic(17), Topic(12)},
       {Topic(18), Topic(12)},   {Topic(19), Topic(12)},
       {Topic(20), Topic(12)},   {Topic(21), Topic(12)},
       {Topic(22), Topic(12)},   {Topic(23), Topic(1)},
       {Topic(24), Topic(23)},   {Topic(25), Topic(23)},
       {Topic(26), Topic(23)},   {Topic(27), Topic(23)},
       {Topic(28), Topic(23)},   {Topic(29), Topic(23)},
       {Topic(30), Topic(23)},   {Topic(31), Topic(23)},
       {Topic(32), Topic(23)},   {Topic(33), Topic(23)},
       {Topic(34), Topic(33)},   {Topic(35), Topic(33)},
       {Topic(36), Topic(33)},   {Topic(37), Topic(23)},
       {Topic(38), Topic(23)},   {Topic(39), Topic(23)},
       {Topic(40), Topic(23)},   {Topic(41), Topic(40)},
       {Topic(42), Topic(1)},    {Topic(43), Topic(1)},
       {Topic(44), Topic(1)},    {Topic(45), Topic(1)},
       {Topic(46), Topic(45)},   {Topic(47), Topic(45)},
       {Topic(48), Topic(45)},   {Topic(49), Topic(48)},
       {Topic(50), Topic(45)},   {Topic(51), Topic(45)},
       {Topic(52), Topic(45)},   {Topic(53), Topic(1)},
       {Topic(54), Topic(53)},   {Topic(55), Topic(53)},
       {Topic(56), Topic(53)},   {Topic(58), Topic(57)},
       {Topic(59), Topic(57)},   {Topic(60), Topic(57)},
       {Topic(61), Topic(57)},   {Topic(62), Topic(57)},
       {Topic(63), Topic(62)},   {Topic(64), Topic(62)},
       {Topic(65), Topic(62)},   {Topic(66), Topic(62)},
       {Topic(67), Topic(62)},   {Topic(68), Topic(62)},
       {Topic(69), Topic(62)},   {Topic(70), Topic(62)},
       {Topic(71), Topic(62)},   {Topic(72), Topic(62)},
       {Topic(73), Topic(62)},   {Topic(74), Topic(62)},
       {Topic(75), Topic(62)},   {Topic(76), Topic(62)},
       {Topic(77), Topic(76)},   {Topic(78), Topic(62)},
       {Topic(79), Topic(57)},   {Topic(80), Topic(57)},
       {Topic(81), Topic(57)},   {Topic(82), Topic(57)},
       {Topic(83), Topic(57)},   {Topic(84), Topic(83)},
       {Topic(85), Topic(57)},   {Topic(87), Topic(86)},
       {Topic(88), Topic(86)},   {Topic(89), Topic(88)},
       {Topic(90), Topic(88)},   {Topic(91), Topic(88)},
       {Topic(92), Topic(88)},   {Topic(93), Topic(88)},
       {Topic(94), Topic(88)},   {Topic(95), Topic(88)},
       {Topic(96), Topic(86)},   {Topic(97), Topic(86)},
       {Topic(98), Topic(97)},   {Topic(99), Topic(86)},
       {Topic(101), Topic(100)}, {Topic(102), Topic(100)},
       {Topic(104), Topic(103)}, {Topic(105), Topic(104)},
       {Topic(106), Topic(103)}, {Topic(107), Topic(106)},
       {Topic(108), Topic(103)}, {Topic(109), Topic(103)},
       {Topic(110), Topic(103)}, {Topic(111), Topic(110)},
       {Topic(112), Topic(110)}, {Topic(113), Topic(103)},
       {Topic(114), Topic(103)}, {Topic(115), Topic(114)},
       {Topic(116), Topic(103)}, {Topic(117), Topic(103)},
       {Topic(118), Topic(117)}, {Topic(119), Topic(103)},
       {Topic(120), Topic(103)}, {Topic(121), Topic(103)},
       {Topic(122), Topic(103)}, {Topic(123), Topic(103)},
       {Topic(124), Topic(103)}, {Topic(125), Topic(103)},
       {Topic(127), Topic(126)}, {Topic(128), Topic(126)},
       {Topic(129), Topic(126)}, {Topic(130), Topic(129)},
       {Topic(131), Topic(129)}, {Topic(132), Topic(129)},
       {Topic(133), Topic(129)}, {Topic(134), Topic(126)},
       {Topic(135), Topic(126)}, {Topic(136), Topic(126)},
       {Topic(137), Topic(126)}, {Topic(138), Topic(137)},
       {Topic(139), Topic(126)}, {Topic(140), Topic(126)},
       {Topic(141), Topic(140)}, {Topic(142), Topic(140)},
       {Topic(143), Topic(140)}, {Topic(144), Topic(140)},
       {Topic(145), Topic(140)}, {Topic(146), Topic(140)},
       {Topic(147), Topic(140)}, {Topic(148), Topic(140)},
       {Topic(150), Topic(149)}, {Topic(151), Topic(150)},
       {Topic(152), Topic(149)}, {Topic(153), Topic(149)},
       {Topic(154), Topic(153)}, {Topic(155), Topic(149)},
       {Topic(156), Topic(155)}, {Topic(157), Topic(149)},
       {Topic(158), Topic(149)}, {Topic(159), Topic(158)},
       {Topic(160), Topic(158)}, {Topic(161), Topic(158)},
       {Topic(162), Topic(158)}, {Topic(163), Topic(158)},
       {Topic(164), Topic(149)}, {Topic(165), Topic(164)},
       {Topic(166), Topic(164)}, {Topic(167), Topic(164)},
       {Topic(168), Topic(164)}, {Topic(169), Topic(164)},
       {Topic(170), Topic(149)}, {Topic(171), Topic(149)},
       {Topic(173), Topic(172)}, {Topic(174), Topic(173)},
       {Topic(175), Topic(173)}, {Topic(176), Topic(175)},
       {Topic(177), Topic(176)}, {Topic(178), Topic(173)},
       {Topic(179), Topic(172)}, {Topic(181), Topic(180)},
       {Topic(182), Topic(180)}, {Topic(183), Topic(180)},
       {Topic(184), Topic(183)}, {Topic(185), Topic(183)},
       {Topic(186), Topic(183)}, {Topic(187), Topic(183)},
       {Topic(188), Topic(183)}, {Topic(189), Topic(183)},
       {Topic(190), Topic(183)}, {Topic(191), Topic(183)},
       {Topic(192), Topic(183)}, {Topic(193), Topic(180)},
       {Topic(194), Topic(180)}, {Topic(195), Topic(180)},
       {Topic(197), Topic(196)}, {Topic(198), Topic(196)},
       {Topic(199), Topic(196)}, {Topic(200), Topic(196)},
       {Topic(201), Topic(196)}, {Topic(202), Topic(201)},
       {Topic(203), Topic(201)}, {Topic(204), Topic(196)},
       {Topic(205), Topic(196)}, {Topic(206), Topic(196)},
       {Topic(208), Topic(207)}, {Topic(209), Topic(207)},
       {Topic(210), Topic(207)}, {Topic(211), Topic(207)},
       {Topic(212), Topic(207)}, {Topic(213), Topic(207)},
       {Topic(214), Topic(207)}, {Topic(216), Topic(215)},
       {Topic(217), Topic(215)}, {Topic(218), Topic(215)},
       {Topic(219), Topic(215)}, {Topic(220), Topic(215)},
       {Topic(221), Topic(215)}, {Topic(222), Topic(215)},
       {Topic(223), Topic(215)}, {Topic(224), Topic(215)},
       {Topic(225), Topic(215)}, {Topic(227), Topic(226)},
       {Topic(228), Topic(227)}, {Topic(229), Topic(227)},
       {Topic(230), Topic(227)}, {Topic(231), Topic(227)},
       {Topic(232), Topic(231)}, {Topic(233), Topic(227)},
       {Topic(234), Topic(227)}, {Topic(235), Topic(227)},
       {Topic(236), Topic(226)}, {Topic(237), Topic(236)},
       {Topic(238), Topic(236)}, {Topic(240), Topic(239)},
       {Topic(241), Topic(239)}, {Topic(242), Topic(241)},
       {Topic(244), Topic(243)}, {Topic(245), Topic(243)},
       {Topic(246), Topic(243)}, {Topic(247), Topic(243)},
       {Topic(248), Topic(243)}, {Topic(249), Topic(243)},
       {Topic(251), Topic(250)}, {Topic(252), Topic(250)},
       {Topic(253), Topic(250)}, {Topic(255), Topic(254)},
       {Topic(256), Topic(255)}, {Topic(257), Topic(255)},
       {Topic(258), Topic(255)}, {Topic(259), Topic(258)},
       {Topic(260), Topic(258)}, {Topic(261), Topic(258)},
       {Topic(262), Topic(254)}, {Topic(264), Topic(263)},
       {Topic(265), Topic(263)}, {Topic(266), Topic(265)},
       {Topic(267), Topic(265)}, {Topic(268), Topic(265)},
       {Topic(269), Topic(265)}, {Topic(270), Topic(265)},
       {Topic(271), Topic(263)}, {Topic(273), Topic(272)},
       {Topic(274), Topic(272)}, {Topic(276), Topic(275)},
       {Topic(277), Topic(275)}, {Topic(278), Topic(275)},
       {Topic(280), Topic(279)}, {Topic(281), Topic(279)},
       {Topic(282), Topic(281)}, {Topic(283), Topic(279)},
       {Topic(284), Topic(279)}, {Topic(285), Topic(279)},
       {Topic(286), Topic(279)}, {Topic(287), Topic(279)},
       {Topic(288), Topic(279)}, {Topic(290), Topic(289)},
       {Topic(291), Topic(289)}, {Topic(292), Topic(289)},
       {Topic(293), Topic(292)}, {Topic(294), Topic(289)},
       {Topic(295), Topic(289)}, {Topic(296), Topic(289)},
       {Topic(297), Topic(289)}, {Topic(298), Topic(289)},
       {Topic(300), Topic(299)}, {Topic(301), Topic(299)},
       {Topic(302), Topic(299)}, {Topic(303), Topic(299)},
       {Topic(304), Topic(299)}, {Topic(305), Topic(299)},
       {Topic(306), Topic(299)}, {Topic(307), Topic(299)},
       {Topic(308), Topic(299)}, {Topic(309), Topic(299)},
       {Topic(310), Topic(299)}, {Topic(311), Topic(299)},
       {Topic(312), Topic(299)}, {Topic(313), Topic(312)},
       {Topic(314), Topic(299)}, {Topic(315), Topic(299)},
       {Topic(316), Topic(299)}, {Topic(317), Topic(299)},
       {Topic(318), Topic(299)}, {Topic(319), Topic(299)},
       {Topic(320), Topic(299)}, {Topic(321), Topic(299)},
       {Topic(322), Topic(299)}, {Topic(323), Topic(299)},
       {Topic(324), Topic(299)}, {Topic(325), Topic(299)},
       {Topic(326), Topic(299)}, {Topic(327), Topic(299)},
       {Topic(328), Topic(299)}, {Topic(329), Topic(299)},
       {Topic(330), Topic(299)}, {Topic(331), Topic(299)},
       {Topic(333), Topic(332)}, {Topic(334), Topic(332)},
       {Topic(335), Topic(332)}, {Topic(336), Topic(332)},
       {Topic(337), Topic(332)}, {Topic(338), Topic(332)},
       {Topic(339), Topic(332)}, {Topic(340), Topic(332)},
       {Topic(341), Topic(332)}, {Topic(342), Topic(332)},
       {Topic(343), Topic(332)}, {Topic(344), Topic(332)},
       {Topic(345), Topic(344)}, {Topic(346), Topic(344)},
       {Topic(347), Topic(344)}, {Topic(348), Topic(344)},
       {Topic(349), Topic(332)}});
  return child_to_parent_map;
}

void GetDescendantTopicsHelper(
    const Topic& topic,
    const std::map<Topic, std::vector<Topic>>& parent_child_map,
    std::set<Topic>& result) {
  auto parent_child_it = parent_child_map.find(topic);
  if (parent_child_it == parent_child_map.end()) {
    return;
  }

  const std::vector<Topic>& child_topics = parent_child_it->second;
  for (const Topic& child_topic : child_topics) {
    if (result.contains(child_topic)) {
      continue;
    }
    result.insert(child_topic);
    GetDescendantTopicsHelper(child_topic, parent_child_map, result);
  }
}

}  // namespace

absl::optional<size_t> GetTaxonomySize() {
  if (blink::features::kBrowsingTopicsTaxonomyVersion.Get() == 1) {
    // Taxonomy version 1 has 349 topics.
    // https://github.com/jkarlin/topics/blob/main/taxonomy_v1.md
    return 349;
  }

  return absl::nullopt;
}

HmacKey GenerateRandomHmacKey() {
  if (g_hmac_key_overridden)
    return GetHmacKeyOverrideForTesting();

  HmacKey result = {};
  base::RandBytes(result.data(), result.size());

  return result;
}

uint64_t HashTopDomainForRandomOrTopTopicDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  int64_t time_microseconds =
      epoch_calculation_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  std::string epoch_id(reinterpret_cast<const char*>(&time_microseconds),
                       sizeof(time_microseconds));

  return HmacHash(hmac_key, kRandomOrTopTopicDecisionPrefix,
                  epoch_id + top_domain);
}

uint64_t HashTopDomainForRandomTopicIndexDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  int64_t time_microseconds =
      epoch_calculation_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  std::string epoch_id(reinterpret_cast<const char*>(&time_microseconds),
                       sizeof(time_microseconds));

  return HmacHash(hmac_key, kRandomTopicIndexDecisionPrefix,
                  epoch_id + top_domain);
}

uint64_t HashTopDomainForTopTopicIndexDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  int64_t time_microseconds =
      epoch_calculation_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  std::string epoch_id(reinterpret_cast<const char*>(&time_microseconds),
                       sizeof(time_microseconds));

  return HmacHash(hmac_key, kTopTopicIndexDecisionPrefix,
                  epoch_id + top_domain);
}

uint64_t HashTopDomainForEpochSwitchTimeDecision(
    ReadOnlyHmacKey hmac_key,
    const std::string& top_domain) {
  return HmacHash(hmac_key, kEpochSwitchTimeDecisionPrefix, top_domain);
}

HashedDomain HashContextDomainForStorage(ReadOnlyHmacKey hmac_key,
                                         const std::string& context_domain) {
  return HashedDomain(
      HmacHash(hmac_key, kContextDomainStoragePrefix, context_domain));
}

HashedHost HashMainFrameHostForStorage(const std::string& main_frame_host) {
  int64_t result;
  crypto::SHA256HashString(kMainFrameHostStoragePrefix + main_frame_host,
                           &result, sizeof(result));
  return HashedHost(result);
}

void OverrideHmacKeyForTesting(ReadOnlyHmacKey hmac_key) {
  g_hmac_key_overridden = true;
  base::ranges::copy(hmac_key, GetHmacKeyOverrideForTesting().begin());
}

std::map<Topic, std::vector<Topic>> GetParentToChildTopicMap() {
  std::map<Topic, std::vector<Topic>> parent_to_child_map;
  for (const auto& [child, parent] : GetChildToParentTopicMap()) {
    parent_to_child_map[parent].emplace_back(child);
  }
  return parent_to_child_map;
}

std::set<Topic> GetDescendantTopics(
    const Topic& topic,
    const std::map<Topic, std::vector<Topic>>& parent_to_child_map) {
  std::set<Topic> descendant_topics;
  GetDescendantTopicsHelper(topic, parent_to_child_map, descendant_topics);
  return descendant_topics;
}

}  // namespace browsing_topics
