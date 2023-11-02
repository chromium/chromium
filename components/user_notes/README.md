# User Notes Component

This directory contains cross-platform business logic for the User Notes
feature.

## Directory structure

- `browser/`
  - Contains business logic classes that live in the browser process.
- `interfaces/`
  - Contains abstract interfaces for contracts between components, as well as
    some shared data structure / container classes.
- `model/`
  - Contains model classes that represent core concepts of the User Notes
    feature, such as a note's metadata, a note's target, a note's body, etc.
- `storage/`
  - Contains classes related to storage and Sync for the notes.
