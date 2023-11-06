// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"

namespace optimization_guide {

// Corresponds to `OptimizationGuidePageEntityCollection` in
// tools/metrics/histograms/enums.xml.
//
// Source of the original collection list:
// https://source.corp.google.com/piper///depot/google3/production/borg/webref/ondevice/model-building-conf-chrome.gcl?q=labelling_collections_hrids
//
// Use `GetPageEntityCollectionForString` to map a raw collection string to its
// enum value.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PageEntityCollection {
  kUnknown = 0,
  kAccommodations = 1,
  kActors = 2,
  kAirports = 3,
  kAnatomicalStructures = 4,
  kArtworks = 5,
  kAthletes = 6,
  kAuthors = 7,
  kBookEditions = 8,
  kBusinessOperations = 9,
  kCars = 10,
  kCausesOfDeath = 11,
  kCelestialObjectWithCoordinateSystems = 12,
  kChemicalCompounds = 13,
  kConsumerProducts = 14,
  kCuisines = 15,
  kCulinaryMeasures = 16,
  kCurrencies = 17,
  kDiets = 18,
  kDiseaseOrMedicalConditions = 19,
  kEducationalInstitutions = 20,
  kEmployers = 21,
  kEvents = 22,
  kFictionalCharacters = 23,
  kFilmActors = 24,
  kFilmScreeningVenues = 25,
  kFilmSeries = 26,
  kFilms = 27,
  kFoods = 28,
  kGarments = 29,
  kGeoBusinessChain = 30,
  kGeoEstablishment = 31,
  kGeoLocality = 32,
  kGeoNaturalFeature = 33,
  kGeoPolitical = 34,
  kHolidays = 35,
  kHumanLanguages = 36,
  kSoftware = 37,
  kJobTitles = 38,
  kLiterarySeries = 39,
  kLocalShoppingBuyables = 40,
  kMaterials = 41,
  kMedicalTreatments = 42,
  kModels = 43,
  kMusicGroupMembers = 44,
  kMusicalAlbums = 45,
  kMusicalArtists = 46,
  kMusicalGenres = 47,
  kMusicalGroups = 48,
  kMusicalRecordings = 49,
  kMusicalReleases = 50,
  kMusicians = 51,
  kOrganismClassifications = 52,
  kOrganizations = 53,
  kPeople = 54,
  kPeriodicals = 55,
  kPoliticians = 56,
  kRecordingClusters = 57,
  kReligions = 58,
  kRestaurants = 59,
  kRideOfferingServices = 60,
  kShoppingCenters = 61,
  kSocialNetworkServiceWebsites = 62,
  kSports = 63,
  kSportsTeams = 64,
  kStructures = 65,
  kTouristAttractions = 66,
  kTravelDestinations = 67,
  kTvActors = 68,
  kTvEpisodes = 69,
  kTvPrograms = 70,
  kVenues = 71,
  kVideoGames = 72,
  kWebsites = 73,
  kWrittenWorks = 74,
  kMaxValue = kWrittenWorks
};

// Returns a collection enum value corresponding to the raw entity collection
// string.
PageEntityCollection GetPageEntityCollectionForString(
    const std::string& collection_str);

// Returns a label for the given raw entity collection string.
std::string_view GetPageEntityCollectionLabel(
    const std::string& collection_str);

// The metadata associated with a single entity.
struct EntityMetadata {
  EntityMetadata();
  EntityMetadata(
      const std::string& entity_id,
      const std::string& human_readable_name,
      const base::flat_map<std::string, float>& human_readable_categories,
      const std::vector<std::string>& human_readable_aliases = {},
      const std::vector<std::string>& collections = {});
  EntityMetadata(const EntityMetadata&);
  ~EntityMetadata();

  // The opaque entity id.
  std::string entity_id;

  // The human-readable name of the entity in the user's locale.
  std::string human_readable_name;

  // A map from human-readable category the entity belongs to in the user's
  // locale to the confidence that the category is related to the entity. Will
  // contain the top 5 entries based on confidence score.
  base::flat_map<std::string, float> human_readable_categories;

  // The ordered set of aliases for this entity in the user's locale.
  std::vector<std::string> human_readable_aliases;

  // A vector of collections of the entity. Will contain the top 5 collections.
  // For UMA metrics, use `GetPageEntityCollectionForString` to convert strings
  // to enum values.
  std::vector<std::string> collections;

  std::string ToString() const;

  std::string ToHumanReadableString() const;

  base::Value AsValue() const;

  friend std::ostream& operator<<(std::ostream& out, const EntityMetadata& md);
  friend bool operator==(const EntityMetadata& lhs, const EntityMetadata& rhs);
};

// The metadata with its score as output of the model execution.
struct ScoredEntityMetadata {
  ScoredEntityMetadata();
  ScoredEntityMetadata(float score, const EntityMetadata& md);
  ScoredEntityMetadata(const ScoredEntityMetadata&);
  ~ScoredEntityMetadata();

  // The metadata.
  EntityMetadata metadata;

  // The score.
  float score;

  std::string ToString() const;

  base::Value AsValue() const;

  friend std::ostream& operator<<(std::ostream& out,
                                  const ScoredEntityMetadata& md);
  friend bool operator==(const ScoredEntityMetadata& lhs,
                         const ScoredEntityMetadata& rhs);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_
