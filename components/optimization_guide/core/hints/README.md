# Optimization Guide Hints

This directory contains the implementation of the hints component of the
Optimization Guide. Optimization "Hints" are small pieces of metadata about
pages that can be fetched from the optimization guide service.  This component
is responsible for fetching, caching, and managing optimization hints.

Most hints can only be fetched from non-incognito profiles that have provided
consent for anonymous data collection, but some simple hints are delivered as
filters in the optimization guide hints component.

## Overview

The core logic resides in `HintsManager`, which is the central class for
managing optimization hints. It is responsible for fetching hints from the remote
Optimization Guide Service, caching them, and providing Decisions and Metadata.

The public-facing API for consumers is the `OptimizationGuideDecider` interface.
This interface is implemented by `OptimizationGuideKeyedService` (in
`//chrome/browser/optimization_guide`), which acts as a facade. The keyed
service owns the `HintsManager` and delegates all decision-making requests to it.

Other key classes in this directory include:
*   `HintsFetcher`: Responsible for fetching hints from the remote service.
*   `HintCache`: Responsible for caching hints in memory and on disk.
*   `OptimizationGuideStore`: The persistent store for hints, implemented using
    a LevelDB database.

## Usage

Consumers should obtain an instance of the `OptimizationGuideDecider` interface
from the `OptimizationGuideKeyedService` to get Decisions and Metadata.

The `HintsManager` (via the `OptimizationGuideKeyedService`) respects user
preferences and policies when fetching and providing hints. For example, hint
fetches are gated by the checks in the
`IsUserPermittedToFetchFromRemoteOptimizationGuide` function, which considers
incognito status and user consent for anonymous data collection.
