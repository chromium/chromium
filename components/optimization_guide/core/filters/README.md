# Optimization Guide Filters

This directory defines a mechanism for using filters over GURLs delivered via
the Optimization Hints component.  These filters are primarily used by the
hints system to determine whether the remote service should be queried, but
may also be exposed as simple hints that don't require any fetch to the service
(which would require anonymous data collection).

## Overview

The main class is `OptimizationFilter`, which represents a filter that can match
against a GURL. The filter can be composed of a Bloom filter and/or a list of
regular expressions.

The `OptimizationHintsComponentUpdateListener` is a singleton that listens for
updates to the Optimization Hints component. When a new component is available,
it notifies its observers.

The `HintsComponentUtil` provides utility functions to process the hints
component and create `OptimizationFilter` instances.

## Usage

This is primarily intended for internal usage by the HintsManager.  See
the README in that directory about public interfaces.
