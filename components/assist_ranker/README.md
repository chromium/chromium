# Assist Ranker

[TOC]

## Introduction

Assist Ranker is design to make Chrome smarter by providing client-side machine
learning (ML) inference in Chrome. It is designed to be a generic infrastructure
that supports ML needs for all Chrome feature teams on all platforms.

Assist Ranker utilizes UKM logging to log per feature-label events. A ML model
will be trained in the Cloud based on these logs; and then Assist Ranker will
download and inference with the model.

It currently only supports Logistic Regression and Multilayer Neural Networks.

Assist Ranker was experiment on ContextualSearch; but it is not used in any
production projects.

## How to use it

Please contact the owners before you use it.