# Alternative state name maps data.

This directory contains alternative state name maps data, one file per country.
Alternative state name maps are used by Autofill to allow recognizing the same
states when referenced by different names. For more details please see
../alternative_state_name_map.h

The files are serialized protocol messages written in binary format.

The proto message can be found in
src/components/autofill/core/browser/proto/states.proto.