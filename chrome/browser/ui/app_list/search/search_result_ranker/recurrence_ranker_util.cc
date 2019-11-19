// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_util.h"

#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {
namespace {

using base::Optional;
using base::Value;

using FakePredictorConfig = RecurrencePredictorConfigProto::FakePredictorConfig;
using DefaultPredictorConfig =
    RecurrencePredictorConfigProto::DefaultPredictorConfig;
using ConditionalFrequencyPredictorConfig =
    RecurrencePredictorConfigProto::ConditionalFrequencyPredictorConfig;
using FrecencyPredictorConfig =
    RecurrencePredictorConfigProto::FrecencyPredictorConfig;
using HourBinPredictorConfig =
    RecurrencePredictorConfigProto::HourBinPredictorConfig;
using MarkovPredictorConfig =
    RecurrencePredictorConfigProto::MarkovPredictorConfig;
using ExponentialWeightsEnsembleConfig =
    RecurrencePredictorConfigProto::ExponentialWeightsEnsembleConfig;

//---------------------
// Conversion utilities
//---------------------

base::Optional<const Value*> GetNestedField(const Value* value,
                                            const std::string& key) {
  const Value* field = value->FindKey(key);
  if (!field || !field->is_dict())
    return base::nullopt;
  return base::Optional<const Value*>(field);
}

Optional<const Value*> GetList(const Value* value, const std::string& key) {
  const Value* field = value->FindKey(key);
  if (!field || !field->is_list())
    return base::nullopt;
  return base::Optional<const Value*>(field);
}

Optional<int> GetInt(const Value* value, const std::string& key) {
  const Value* field = value->FindKey(key);
  if (!field || !field->is_int())
    return base::nullopt;
  return field->GetInt();
}

base::Optional<double> GetDouble(const Value* value, const std::string& key) {
  const Value* field = value->FindKey(key);
  if (!field || !field->is_double())
    return base::nullopt;
  return field->GetDouble();
}

base::Optional<std::string> GetString(const Value* value,
                                      const std::string& key) {
  const Value* field = value->FindKey(key);
  if (!field || !field->is_string())
    return base::nullopt;
  return field->GetString();
}

//----------------------
// Predictor conversions
//----------------------

bool ConvertRecurrencePredictor(const Value*,
                                RecurrencePredictorConfigProto* proto);

bool ConvertFrecencyPredictor(const Value* value,
                              FrecencyPredictorConfig* proto) {
  const auto& decay_coeff = GetDouble(value, "decay_coeff");
  if (!decay_coeff)
    return false;
  proto->set_decay_coeff(decay_coeff.value());
  return true;
}

bool ConvertHourBinPredictor(const Value* value,
                             HourBinPredictorConfig* proto) {
  const auto& bin_weights = GetList(value, "bin_weights");

  if (!bin_weights)
    return false;

  for (const Value& bin_weight : bin_weights.value()->GetList()) {
    const auto& bin = GetInt(&bin_weight, "bin");
    const auto& weight = GetDouble(&bin_weight, "weight");
    if (!bin || !weight)
      return false;

    auto* proto_bin_weight = proto->add_bin_weights();
    proto_bin_weight->set_bin(bin.value());
    proto_bin_weight->set_weight(weight.value());
  }
  return true;
}

bool ConvertExponentialWeightsEnsemble(
    const Value* value,
    ExponentialWeightsEnsembleConfig* proto) {
  const auto& learning_rate = GetDouble(value, "learning_rate");
  const auto& predictors = GetList(value, "predictors");

  if (!learning_rate || !predictors)
    return false;

  proto->set_learning_rate(learning_rate.value());

  bool success = true;
  for (const Value& predictor : predictors.value()->GetList())
    success &= ConvertRecurrencePredictor(&predictor, proto->add_predictors());
  return success;
}

//----------------------
// Framework conversions
//----------------------

bool ConvertRecurrenceRanker(const Value* value,
                             RecurrenceRankerConfigProto* proto) {
  const auto& min_seconds_between_saves =
      GetInt(value, "min_seconds_between_saves");
  const auto& target_limit = GetInt(value, "target_limit");
  const auto& target_decay = GetDouble(value, "target_decay");
  const auto& condition_limit = GetInt(value, "condition_limit");
  const auto& condition_decay = GetDouble(value, "condition_decay");
  const auto& predictor = GetNestedField(value, "predictor");

  if (!min_seconds_between_saves || !target_limit || !target_decay ||
      !condition_limit || !condition_decay || !predictor)
    return false;

  proto->set_min_seconds_between_saves(min_seconds_between_saves.value());
  proto->set_target_limit(target_limit.value());
  proto->set_target_decay(target_decay.value());
  proto->set_condition_limit(condition_limit.value());
  proto->set_condition_decay(condition_decay.value());

  return ConvertRecurrencePredictor(predictor.value(),
                                    proto->mutable_predictor());
}

bool ConvertRecurrencePredictor(const Value* value,
                                RecurrencePredictorConfigProto* proto) {
  const auto& predictor_type = GetString(value, "predictor_type");
  if (!predictor_type)
    return false;

  // Add new predictor converters here. Predictors with parameters should call a
  // ConvertX function, and predictors without parameters should just set an
  // empty message for that predictor. The empty message is important because
  // its existence determines which predictor to use.
  if (predictor_type == "fake") {
    proto->mutable_fake_predictor();
    return true;
  } else if (predictor_type == "default") {
    proto->mutable_default_predictor();
    return true;
  } else if (predictor_type == "conditional frequency") {
    proto->mutable_conditional_frequency_predictor();
    return true;
  } else if (predictor_type == "frecency") {
    return ConvertFrecencyPredictor(value, proto->mutable_frecency_predictor());
  } else if (predictor_type == "hour bin") {
    return ConvertHourBinPredictor(value, proto->mutable_hour_bin_predictor());
  } else if (predictor_type == "markov") {
    proto->mutable_markov_predictor();
    return true;
  } else if (predictor_type == "exponential weights ensemble") {
    return ConvertExponentialWeightsEnsemble(
        value, proto->mutable_exponential_weights_ensemble());
  } else {
    return false;
  }
}

}  // namespace

std::unique_ptr<RecurrencePredictor> MakePredictor(
    const RecurrencePredictorConfigProto& config,
    const std::string& model_identifier) {
  if (config.has_fake_predictor())
    return std::make_unique<FakePredictor>(config.fake_predictor(),
                                           model_identifier);
  if (config.has_default_predictor())
    return std::make_unique<DefaultPredictor>(config.default_predictor(),
                                              model_identifier);
  if (config.has_conditional_frequency_predictor())
    return std::make_unique<ConditionalFrequencyPredictor>(

        config.conditional_frequency_predictor(), model_identifier);
  if (config.has_frecency_predictor())
    return std::make_unique<FrecencyPredictor>(config.frecency_predictor(),
                                               model_identifier);
  if (config.has_hour_bin_predictor())
    return std::make_unique<HourBinPredictor>(config.hour_bin_predictor(),
                                              model_identifier);
  if (config.has_markov_predictor())
    return std::make_unique<MarkovPredictor>(config.markov_predictor(),
                                             model_identifier);
  if (config.has_exponential_weights_ensemble())
    return std::make_unique<ExponentialWeightsEnsemble>(
        config.exponential_weights_ensemble(), model_identifier);

  LogInitializationStatus(model_identifier,
                          InitializationStatus::kInvalidConfigPredictor);
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<JsonConfigConverter> JsonConfigConverter::Convert(
    const std::string& json_string,
    const std::string& model_identifier,
    OnConfigLoadedCallback callback) {
  // We don't use make_unique because the ctor is private.
  std::unique_ptr<JsonConfigConverter> converter(new JsonConfigConverter());
  converter->Start(json_string, model_identifier, std::move(callback));
  return converter;
}

JsonConfigConverter::JsonConfigConverter() = default;

JsonConfigConverter::~JsonConfigConverter() = default;

void JsonConfigConverter::Start(const std::string& json_string,
                                const std::string& model_identifier,
                                OnConfigLoadedCallback callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      json_string, base::BindOnce(&JsonConfigConverter::OnJsonParsed,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback), model_identifier));
}

void JsonConfigConverter::OnJsonParsed(
    OnConfigLoadedCallback callback,
    const std::string& model_identifier,
    data_decoder::DataDecoder::ValueOrError result) {
  RecurrenceRankerConfigProto proto;
  if (result.value && ConvertRecurrenceRanker(&result.value.value(), &proto)) {
    LogJsonConfigConversionStatus(model_identifier,
                                  JsonConfigConversionStatus::kSuccess);
    std::move(callback).Run(std::move(proto));
  } else {
    LogJsonConfigConversionStatus(model_identifier,
                                  JsonConfigConversionStatus::kFailure);
    std::move(callback).Run(base::nullopt);
  }
}

}  // namespace app_list
