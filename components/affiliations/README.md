# Affiliations

Affiliations is a component that allows querying an affiliation service backend.
Affiliations are used to link certain types of origin-specific data across
multiple origins - for example passwords that should be available on two
different domains belonging to the same entity or passwords that should be
shared between a website and the native Android application.

## Terminology

A "facet" is defined as the manifestation of a logical application on a given
platform. For example, "My Bank" may have released an Android application and a
Web application accessible from a browser. These are all facets of the "My Bank"
logical application.

Facets that belong to the same logical application are said to be affiliated
with each other. Conceptually, "affiliations" can be seen as an equivalence
relation defined over the set of all facets. Each equivalence class contains
facets that belong to the same logical application, and therefore should be
treated as synonymous for certain purposes, e.g., sharing credentials.

## Key classes

- `AffiliationService` (implemented by `AffiliationServiceImpl`): The key class
  that most outside users should access to retrieve affiliations. In practice,
  it is mostly a wrapper around `AffiliationBackend`.
- `AffiliationBackend`: Performs (potentially blocking) I/O, both for network
  requests and for database lookups.
- `AffiliationDatabase`: Stores and accesses cached affiliation data in a local
  SQL database.
- `FacetManager`: Responsible for retrieving affiliations data for a single
  facet, either from the local cache or by triggering network requests to the
  affiliations server.
