// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/entity_metadata.h"

#include <ostream>
#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace optimization_guide {

PageEntityCollection GetPageEntityCollectionForString(
    const std::string& collection_str) {
  // A const map of raw entity collection strings to enum values.
  //
  // The map keys need to be kept consistent with the source of the original
  // collection list:
  // https://source.corp.google.com/piper///depot/google3/production/borg/webref/ondevice/model-building-conf-chrome.gcl?q=labelling_collections_hrids
  static const base::flat_map<std::string, PageEntityCollection>
      kPageEntityCollectionMap = {
          {"/collection/accommodations", PageEntityCollection::kAccommodations},
          {"/collection/actors", PageEntityCollection::kActors},
          {"/collection/airports", PageEntityCollection::kAirports},
          {"/collection/anatomical_structures",
           PageEntityCollection::kAnatomicalStructures},
          {"/collection/artworks", PageEntityCollection::kArtworks},
          {"/collection/athletes", PageEntityCollection::kAthletes},
          {"/collection/authors", PageEntityCollection::kAuthors},
          {"/collection/book_editions", PageEntityCollection::kBookEditions},
          {"/collection/business_operations",
           PageEntityCollection::kBusinessOperations},
          {"/collection/cars", PageEntityCollection::kCars},
          {"/collection/causes_of_death", PageEntityCollection::kCausesOfDeath},
          {"/collection/celestial_object_with_coordinate_systems",
           PageEntityCollection::kCelestialObjectWithCoordinateSystems},
          {"/collection/chemical_compounds",
           PageEntityCollection::kChemicalCompounds},
          {"/collection/consumer_products",
           PageEntityCollection::kConsumerProducts},
          {"/collection/cuisines", PageEntityCollection::kCuisines},
          {"/collection/culinary_measures",
           PageEntityCollection::kCulinaryMeasures},
          {"/collection/currencies", PageEntityCollection::kCurrencies},
          {"/collection/diets", PageEntityCollection::kDiets},
          {"/collection/disease_or_medical_conditions",
           PageEntityCollection::kDiseaseOrMedicalConditions},
          {"/collection/educational_institutions",
           PageEntityCollection::kEducationalInstitutions},
          {"/collection/employers", PageEntityCollection::kEmployers},
          {"/collection/events", PageEntityCollection::kEvents},
          {"/collection/fictional_characters",
           PageEntityCollection::kFictionalCharacters},
          {"/collection/film_actors", PageEntityCollection::kFilmActors},
          {"/collection/film_screening_venues",
           PageEntityCollection::kFilmScreeningVenues},
          {"/collection/film_series", PageEntityCollection::kFilmSeries},
          {"/collection/films", PageEntityCollection::kFilms},
          {"/collection/foods", PageEntityCollection::kFoods},
          {"/collection/garments", PageEntityCollection::kGarments},
          {"/collection/geo/business_chain",
           PageEntityCollection::kGeoBusinessChain},
          {"/collection/geo/establishment",
           PageEntityCollection::kGeoEstablishment},
          {"/collection/geo/locality", PageEntityCollection::kGeoLocality},
          {"/collection/geo/natural_feature",
           PageEntityCollection::kGeoNaturalFeature},
          {"/collection/geo/political", PageEntityCollection::kGeoPolitical},
          {"/collection/holidays", PageEntityCollection::kHolidays},
          {"/collection/human_languages",
           PageEntityCollection::kHumanLanguages},
          {"/collection/software", PageEntityCollection::kSoftware},
          {"/collection/job_titles", PageEntityCollection::kJobTitles},
          {"/collection/literary_series",
           PageEntityCollection::kLiterarySeries},
          {"/collection/local_shopping_buyables",
           PageEntityCollection::kLocalShoppingBuyables},
          {"/collection/materials", PageEntityCollection::kMaterials},
          {"/collection/medical_treatments",
           PageEntityCollection::kMedicalTreatments},
          {"/collection/models", PageEntityCollection::kModels},
          {"/collection/music_group_members",
           PageEntityCollection::kMusicGroupMembers},
          {"/collection/musical_albums", PageEntityCollection::kMusicalAlbums},
          {"/collection/musical_artists",
           PageEntityCollection::kMusicalArtists},
          {"/collection/musical_genres", PageEntityCollection::kMusicalGenres},
          {"/collection/musical_groups", PageEntityCollection::kMusicalGroups},
          {"/collection/musical_recordings",
           PageEntityCollection::kMusicalRecordings},
          {"/collection/musical_releases",
           PageEntityCollection::kMusicalReleases},
          {"/collection/musicians", PageEntityCollection::kMusicians},
          {"/collection/organism_classifications",
           PageEntityCollection::kOrganismClassifications},
          {"/collection/organizations", PageEntityCollection::kOrganizations},
          {"/collection/people", PageEntityCollection::kPeople},
          {"/collection/periodicals", PageEntityCollection::kPeriodicals},
          {"/collection/politicians", PageEntityCollection::kPoliticians},
          {"/collection/recording_clusters",
           PageEntityCollection::kRecordingClusters},
          {"/collection/religions", PageEntityCollection::kReligions},
          {"/collection/restaurants", PageEntityCollection::kRestaurants},
          {"/collection/ride_offering_services",
           PageEntityCollection::kRideOfferingServices},
          {"/collection/shopping_centers",
           PageEntityCollection::kShoppingCenters},
          {"/collection/social_network_service_websites",
           PageEntityCollection::kSocialNetworkServiceWebsites},
          {"/collection/sports", PageEntityCollection::kSports},
          {"/collection/sports_teams", PageEntityCollection::kSportsTeams},
          {"/collection/structures", PageEntityCollection::kStructures},
          {"/collection/tourist_attractions",
           PageEntityCollection::kTouristAttractions},
          {"/collection/travel_destinations",
           PageEntityCollection::kTravelDestinations},
          {"/collection/tv_actors", PageEntityCollection::kTvActors},
          {"/collection/tv_episodes", PageEntityCollection::kTvEpisodes},
          {"/collection/tv_programs", PageEntityCollection::kTvPrograms},
          {"/collection/venues", PageEntityCollection::kVenues},
          {"/collection/video_games", PageEntityCollection::kVideoGames},
          {"/collection/websites", PageEntityCollection::kWebsites},
          {"/collection/written_works", PageEntityCollection::kWrittenWorks}};

  const auto it = kPageEntityCollectionMap.find(collection_str);
  return it != kPageEntityCollectionMap.end() ? it->second
                                              : PageEntityCollection::kUnknown;
}

std::string GetPageEntityCollectionLabel(const std::string& collection_str) {
  static constexpr char kUnknown[] = "Unknown";
  // A const map of raw entity collection strings to labels.
  //
  // The map keys need to be kept consistent with the source of the original
  // collection list:
  // https://source.corp.google.com/piper///depot/google3/production/borg/webref/ondevice/model-building-conf-chrome.gcl?q=labelling_collections_hrids
  static const base::flat_map<std::string, std::string>
      kPageEntityCollectionLabelMap = {
          {"/collection/accommodations", "Accommodations"},
          {"/collection/actors", "Actors"},
          {"/collection/airports", "Airports"},
          {"/collection/anatomical_structures", "AnatomicalStructures"},
          {"/collection/artworks", "Artworks"},
          {"/collection/athletes", "Athletes"},
          {"/collection/authors", "Authors"},
          {"/collection/book_editions", "BookEditions"},
          {"/collection/business_operations", "BusinessOperations"},
          {"/collection/cars", "Cars"},
          {"/collection/causes_of_death", "CausesOfDeath"},
          {"/collection/celestial_object_with_coordinate_systems",
           "CelestialObjectWithCoordinateSystems"},
          {"/collection/chemical_compounds", "HemicalCompounds"},
          {"/collection/consumer_products", "ConsumerProducts"},
          {"/collection/cuisines", "Cuisines"},
          {"/collection/culinary_measures", "CulinaryMeasures"},
          {"/collection/currencies", "Currencies"},
          {"/collection/diets", "Diets"},
          {"/collection/disease_or_medical_conditions",
           "DiseaseOrMedicalConditions"},
          {"/collection/educational_institutions", "EducationalInstitutions"},
          {"/collection/employers", "Employers"},
          {"/collection/events", "Events"},
          {"/collection/fictional_characters", "FictionalCharacters"},
          {"/collection/film_actors", "FilmActors"},
          {"/collection/film_screening_venues", "FilmScreeningVenues"},
          {"/collection/film_series", "FilmSeries"},
          {"/collection/films", "Films"},
          {"/collection/foods", "Foods"},
          {"/collection/garments", "Garments"},
          {"/collection/geo/business_chain", "GeoBusinessChain"},
          {"/collection/geo/establishment", "GeoEstablishment"},
          {"/collection/geo/locality", "GeoLocality"},
          {"/collection/geo/natural_feature", "GeoNaturalFeature"},
          {"/collection/geo/political", "GeoPolitical"},
          {"/collection/holidays", "Holidays"},
          {"/collection/human_languages", "HumanLanguages"},
          {"/collection/software", "Software"},
          {"/collection/job_titles", "JobTitles"},
          {"/collection/literary_series", "LiterarySeries"},
          {"/collection/local_shopping_buyables", "LocalShoppingBuyables"},
          {"/collection/materials", "Materials"},
          {"/collection/medical_treatments", "MedicalTreatments"},
          {"/collection/models", "Models"},
          {"/collection/music_group_members", "MusicGroupMembers"},
          {"/collection/musical_albums", "MusicalAlbums"},
          {"/collection/musical_artists", "MusicalArtists"},
          {"/collection/musical_genres", "MusicalGenres"},
          {"/collection/musical_groups", "MusicalGroups"},
          {"/collection/musical_recordings", "MusicalRecordings"},
          {"/collection/musical_releases", "MusicalReleases"},
          {"/collection/musicians", "Musicians"},
          {"/collection/organism_classifications", "OrganismClassifications"},
          {"/collection/organizations", "Organizations"},
          {"/collection/people", "People"},
          {"/collection/periodicals", "Periodicals"},
          {"/collection/politicians", "Politicians"},
          {"/collection/recording_clusters", "RecordingClusters"},
          {"/collection/religions", "Religions"},
          {"/collection/restaurants", "Restaurants"},
          {"/collection/ride_offering_services", "RideOfferingServices"},
          {"/collection/shopping_centers", "ShoppingCenters"},
          {"/collection/social_network_service_websites",
           "SocialNetworkServiceWebsites"},
          {"/collection/sports", "Sports"},
          {"/collection/sports_teams", "SportsTeams"},
          {"/collection/structures", "Structures"},
          {"/collection/tourist_attractions", "TouristAttractions"},
          {"/collection/travel_destinations", "TravelDestinations"},
          {"/collection/tv_actors", "TvActors"},
          {"/collection/tv_episodes", "TvEpisodes"},
          {"/collection/tv_programs", "TvPrograms"},
          {"/collection/venues", "Venues"},
          {"/collection/video_games", "VideoGames"},
          {"/collection/websites", "Websites"},
          {"/collection/written_works", "WrittenWorks"}};

  const auto it = kPageEntityCollectionLabelMap.find(collection_str);
  if (it != kPageEntityCollectionLabelMap.end()) {
    return it->second;
  } else {
    return kUnknown;
  }
}

EntityMetadata::EntityMetadata() = default;
EntityMetadata::EntityMetadata(
    const std::string& entity_id,
    const std::string& human_readable_name,
    const base::flat_map<std::string, float>& human_readable_categories,
    const std::vector<std::string>& human_readable_aliases,
    const std::vector<std::string>& collections)
    : entity_id(entity_id),
      human_readable_name(human_readable_name),
      human_readable_categories(human_readable_categories),
      human_readable_aliases(human_readable_aliases),
      collections(collections) {}
EntityMetadata::EntityMetadata(const EntityMetadata&) = default;
EntityMetadata::~EntityMetadata() = default;

base::Value EntityMetadata::AsValue() const {
  base::Value::List categories;
  for (const auto& iter : human_readable_categories) {
    base::Value::Dict category;
    category.Set("category", iter.first);
    category.Set("score", iter.second);
    categories.Append(std::move(category));
  }
  base::Value::List aliases_list;
  for (const auto& alias : human_readable_aliases) {
    aliases_list.Append(alias);
  }
  base::Value::List collection_list;
  for (const auto& collection : collections) {
    collection_list.Append(collection);
  }

  base::Value::Dict metadata;
  metadata.Set("entity_id", entity_id);
  metadata.Set("human_readable_name", human_readable_name);
  metadata.Set("categories", std::move(categories));
  metadata.Set("human_readable_aliases", std::move(aliases_list));
  metadata.Set("collections", std::move(collection_list));

  return base::Value(std::move(metadata));
}

std::string EntityMetadata::ToString() const {
  std::vector<std::string> categories;
  for (const auto& iter : human_readable_categories) {
    categories.push_back(
        base::StringPrintf("{%s,%f}", iter.first.c_str(), iter.second));
  }

  return base::StringPrintf(
      "EntityMetadata{%s, %s, {%s}, {%s}, {%s}}", entity_id.c_str(),
      human_readable_name.c_str(), base::JoinString(categories, ",").c_str(),
      base::JoinString(human_readable_aliases, ",").c_str(),
      base::JoinString(collections, ",").c_str());
}

std::string EntityMetadata::ToHumanReadableString() const {
  std::vector<std::string> categories;
  for (const auto& iter : human_readable_categories) {
    categories.push_back(
        base::StringPrintf("{%s,%f}", iter.first.c_str(), iter.second));
  }

  return base::StringPrintf(
      "%s %s, %s {%s}, %s {%s}, %s {%s}",
      "Entity:", human_readable_name.c_str(),
      "Categories: ", base::JoinString(categories, ",").c_str(),
      "Aliases: ", base::JoinString(human_readable_aliases, ",").c_str(),
      "Collections: ", base::JoinString(collections, ",").c_str());
}

std::ostream& operator<<(std::ostream& out, const EntityMetadata& md) {
  out << md.ToString();
  return out;
}

bool operator==(const EntityMetadata& lhs, const EntityMetadata& rhs) {
  return lhs.entity_id == rhs.entity_id &&
         lhs.human_readable_name == rhs.human_readable_name &&
         lhs.human_readable_categories == rhs.human_readable_categories &&
         lhs.human_readable_aliases == rhs.human_readable_aliases &&
         lhs.collections == rhs.collections;
}

ScoredEntityMetadata::ScoredEntityMetadata() = default;
ScoredEntityMetadata::ScoredEntityMetadata(float score,
                                           const EntityMetadata& md)
    : metadata(md), score(score) {}
ScoredEntityMetadata::ScoredEntityMetadata(const ScoredEntityMetadata&) =
    default;
ScoredEntityMetadata::~ScoredEntityMetadata() = default;

base::Value ScoredEntityMetadata::AsValue() const {
  base::Value::Dict scored_md;
  scored_md.Set("metadata", metadata.AsValue());
  scored_md.Set("score", score);
  return base::Value(std::move(scored_md));
}

std::string ScoredEntityMetadata::ToString() const {
  return base::StringPrintf("ScoredEntityMetadata{%f, %s}", score,
                            metadata.ToString().c_str());
}

std::ostream& operator<<(std::ostream& out, const ScoredEntityMetadata& md) {
  out << md.ToString();
  return out;
}

bool operator==(const ScoredEntityMetadata& lhs,
                const ScoredEntityMetadata& rhs) {
  constexpr const double kScoreTolerance = 1e-6;
  return lhs.metadata == rhs.metadata &&
         std::abs(lhs.score - rhs.score) <= kScoreTolerance;
}

}  // namespace optimization_guide
