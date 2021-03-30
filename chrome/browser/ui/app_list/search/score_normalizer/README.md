# Launcher Results Score Normalization
This directory contains the score normalizer and balanced reservoir for launcher score normalization. Different provider scores are of different distributions, they can be normalized with the score normalizer to a value in a uniform distribution between 0 and 1 to be directly comparable in the launcher. The normalization procedure used is quantile transformation, where scores are mapped to their quantile in a balanced reservoir.

## Authors
adafang
wrong
tby